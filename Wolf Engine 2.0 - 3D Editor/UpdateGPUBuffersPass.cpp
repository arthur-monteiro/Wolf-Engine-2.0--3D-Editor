#include "UpdateGPUBuffersPass.h"

#include <Configuration.h>
#include <DebugMarker.h>
#include <ProfilerCommon.h>
#include <RuntimeContext.h>

#include <GPUDataTransfersManager.h>

void UpdateGPUBuffersPass::initializeResources(const Wolf::InitializationContext& context)
{
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::TRANSFER, false));
	createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

	m_currentRequestsQueues.resize(Wolf::g_configuration->getMaxCachedFrames());
	m_stagingBufferPool.reset(new StagingBufferPool(268'435'456));
}

void UpdateGPUBuffersPass::resize(const Wolf::InitializationContext& context)
{
	// Nothing to do
}

void UpdateGPUBuffersPass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	m_stagingBufferPool->garbageCollect();

	uint32_t requestQueueIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();

	m_addRequestQueueMutex.lock();
	{
		{
			PROFILE_SCOPED("Swap requests")
			m_currentRequestsQueues[requestQueueIdx].swap(m_addRequestsQueue);
		}
		{
			PROFILE_SCOPED("Clear add queue")
			m_addRequestsQueue.clear();
		}

	}
	m_addRequestQueueMutex.unlock();

	if (m_currentRequestsQueues[requestQueueIdx].empty())
	{
		m_transferRecordedThisFrame = false;
		return;
	}

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Update GPU buffers pass");

	for (uint32_t i = 0; i < m_currentRequestsQueues[requestQueueIdx].size(); ++i)
	{
		const Wolf::ResourceUniqueOwner<InternalRequest>& request = m_currentRequestsQueues[requestQueueIdx][i];
		request->recordCommands(m_commandBuffer.get());
	}

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();

	m_transferRecordedThisFrame = true;
}

void UpdateGPUBuffersPass::submit(const Wolf::SubmitContext& context)
{
	if (m_transferRecordedThisFrame)
	{
		std::vector<const Wolf::Semaphore*> waitSemaphores{ };
		const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
		m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
	}
}

void UpdateGPUBuffersPass::clear()
{
	m_addRequestsQueue.clear();
	m_currentRequestsQueues.clear();
}

void UpdateGPUBuffersPass::InternalRequest::recordCommands(const Wolf::CommandBuffer* commandBuffer) const
{
	PROFILE_FUNCTION

	if (m_mode == Mode::FILL)
	{
		if (m_request.getResourceType() != Request::ResourceType::BUFFER)
			Wolf::Debug::sendCriticalError("Filling something else than a buffer is not supported");

		recordFillBuffer(commandBuffer);
	}
	else
	{
		switch (m_request.getResourceType())
		{
			case Request::ResourceType::BUFFER:
				recordCopyToBuffer(commandBuffer);
				break;
			case Request::ResourceType::IMAGE:
				recordCopyToImage(commandBuffer);
				break;
			default:
				Wolf::Debug::sendCriticalError("Unhandled resource type");
		}
	}
}

void UpdateGPUBuffersPass::InternalRequest::recordCopyToBuffer(const Wolf::CommandBuffer* commandBuffer) const
{
	Wolf::Buffer::BufferCopy bufferCopy{};
	bufferCopy.srcOffset = m_stagingBufferPoolInstance.m_bufferOffset;
	bufferCopy.dstOffset = m_request.getOutputOffset();
	bufferCopy.size = m_request.getSize();

	m_request.getOutputBuffer()->recordTransferGPUMemory(commandBuffer, *m_stagingBufferPool->getBuffer(m_stagingBufferPoolInstance), bufferCopy);
}

void UpdateGPUBuffersPass::InternalRequest::recordFillBuffer(const Wolf::CommandBuffer* commandBuffer) const
{
	Wolf::Buffer::BufferFill bufferFill{};
	bufferFill.dstOffset = m_request.getOutputOffset();
	bufferFill.size = m_request.getSize();
	bufferFill.data = m_request.getFillData();

	m_request.getOutputBuffer()->recordFillBuffer(commandBuffer, bufferFill);
}

void UpdateGPUBuffersPass::InternalRequest::recordCopyToImage(const Wolf::CommandBuffer* commandBuffer) const
{
	Wolf::Image::BufferImageCopy bufferImageCopy;
	bufferImageCopy.bufferOffset = m_stagingBufferPoolInstance.m_bufferOffset;
	bufferImageCopy.bufferRowLength = m_request.getCopySize().x;
	bufferImageCopy.bufferImageHeight = m_request.getCopySize().y;

	bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferImageCopy.imageSubresource.mipLevel = m_request.getMipLevel();
	bufferImageCopy.imageSubresource.baseArrayLayer = 0;
	bufferImageCopy.imageSubresource.layerCount = 1;

	bufferImageCopy.imageOffset = { m_request.getImageOffset().x, m_request.getImageOffset().y, m_request.getImageOffset().z };
	bufferImageCopy.imageExtent = { static_cast<uint32_t>(m_request.getCopySize().x), static_cast<uint32_t>(m_request.getCopySize().y), static_cast<uint32_t>(m_request.getCopySize().z)};

	m_request.getOutputImage()->recordCopyGPUBuffer(*commandBuffer, *m_stagingBufferPool->getBuffer(m_stagingBufferPoolInstance), bufferImageCopy, m_request.getImageFinalLayout());
}

void UpdateGPUBuffersPass::addRequestBeforeFrame(const Request& request)
{
	PROFILE_FUNCTION

	InternalRequest* newInternalRequest = new InternalRequest(request, m_stagingBufferPool.createNonOwnerResource());
	m_addRequestQueueMutex.lock();
	m_addRequestsQueue.emplace_back(newInternalRequest);
	m_addRequestQueueMutex.unlock();
}

UpdateGPUBuffersPass::StagingBufferPool::StagingBufferPool(uint32_t poolSize) : m_poolSize(poolSize)
{
	allocateNewBuffer();
}

Wolf::BufferPoolInterface::BufferPoolInstance UpdateGPUBuffersPass::StagingBufferPool::allocate(uint32_t requestedSize, Wolf::Buffer::BufferUsageFlags usageFlags, uint32_t itemSize)
{
	if (requestedSize > m_poolSize)
	{
		Wolf::Debug::sendCriticalError("Requested size is too big to fit in staging buffer");
	}

	BufferPoolInstance r{};

	m_mutex.lock();

	r.m_bufferIdx = 0;
	bool spaceFound = false;
	while (!spaceFound && r.m_bufferIdx < m_buffers.size())
	{
		OwningBuffer& buffer = m_buffers[r.m_bufferIdx];

		uint32_t allocationOffset = buffer.m_currentAllocatedOffset;
		buffer.m_currentAllocatedOffset += requestedSize;

		if (buffer.m_currentAllocatedOffset > buffer.m_buffer->getSize())
		{
			if (buffer.m_currentDeletedOffset < requestedSize)
			{
				r.m_bufferIdx++;
				continue;
			}

			allocationOffset = buffer.m_currentAllocatedOffset = 0;
			buffer.m_currentDeletedOffset = 0;
		}
		r.m_bufferOffset = allocationOffset;
		r.m_bufferSize = requestedSize;

		spaceFound = true;
	}

	if (!spaceFound)
	{
		r.m_bufferIdx = m_buffers.size();
		allocateNewBuffer();
		m_buffers[r.m_bufferIdx].m_currentAllocatedOffset += requestedSize;
		r.m_bufferOffset = 0;
		r.m_bufferSize = requestedSize;
	}

	m_buffers[r.m_bufferIdx].m_activeAllocations++;

	m_mutex.unlock();

	return r;
}

void UpdateGPUBuffersPass::StagingBufferPool::deallocate(const BufferPoolInstance& bufferPoolInstance)
{
	OwningBuffer& owningBuffer = m_buffers[bufferPoolInstance.m_bufferIdx];

	m_mutex.lock();
	owningBuffer.m_currentDeletedOffset = std::max(owningBuffer.m_currentDeletedOffset, bufferPoolInstance.m_bufferOffset + bufferPoolInstance.m_bufferSize);
	if (owningBuffer.m_currentDeletedOffset > owningBuffer.m_buffer->getSize())
	{
		Wolf::Debug::sendCriticalError("It shouldn't happen");
	}
	if (owningBuffer.m_activeAllocations == 0)
	{
		Wolf::Debug::sendCriticalError("Active allocations should not be zero");
	}
	owningBuffer.m_activeAllocations--;

	if (owningBuffer.m_activeAllocations == 0)
	{
		owningBuffer.m_currentDeletedOffset = owningBuffer.m_currentAllocatedOffset = 0;
	}

	m_mutex.unlock();
}

void UpdateGPUBuffersPass::StagingBufferPool::garbageCollect()
{
	m_mutex.lock();

	for (int32_t i = m_buffers.size() - 1; i >= 0; i--)
	{
		OwningBuffer& owningBuffer = m_buffers[i];

		if (owningBuffer.m_activeAllocations == 0)
		{
			m_buffers.erase(m_buffers.begin() + i);
		}
		else
			break; // don't delete previous indices as this index will change
	}

	m_mutex.unlock();
}

Wolf::ResourceNonOwner<Wolf::Buffer> UpdateGPUBuffersPass::StagingBufferPool::getBuffer(const BufferPoolInstance& bufferPoolInstance)
{
	return m_buffers[bufferPoolInstance.m_bufferIdx].m_buffer.createNonOwnerResource();
}

void UpdateGPUBuffersPass::StagingBufferPool::allocateNewBuffer()
{
	OwningBuffer& owningBuffer = m_buffers.emplace_back();
	owningBuffer.m_buffer.reset(Wolf::Buffer::createBuffer(m_poolSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	owningBuffer.m_buffer->setName("Staging buffer for data copy (UpdateGPUBuffersPass::StagingBufferPool::m_buffer)");
}
