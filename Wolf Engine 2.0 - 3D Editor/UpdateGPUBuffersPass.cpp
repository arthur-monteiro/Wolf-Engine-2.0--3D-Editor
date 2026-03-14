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

	bufferImageCopy.imageOffset = { m_request.getImageOffset().x, m_request.getImageOffset().y, 0 };
	bufferImageCopy.imageExtent = { static_cast<uint32_t>(m_request.getCopySize().x), static_cast<uint32_t>(m_request.getCopySize().y), 1};

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

UpdateGPUBuffersPass::StagingBufferPool::StagingBufferPool(uint32_t poolSize)
{
	m_buffer.reset(Wolf::Buffer::createBuffer(poolSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	m_buffer->setName("Staging buffer for data copy (UpdateGPUBuffersPass::StagingBufferPool::m_buffer)");
}

Wolf::BufferPoolInterface::BufferPoolInstance UpdateGPUBuffersPass::StagingBufferPool::allocate(uint32_t requestedSize, Wolf::Buffer::BufferUsageFlags usageFlags, uint32_t itemSize)
{
	BufferPoolInstance r{};
	r.m_bufferIdx = 0;

	m_mutex.lock();
	uint32_t allocationOffset = m_currentAllocatedOffset;
	m_currentAllocatedOffset += requestedSize;
	m_mutex.unlock();

	if (m_currentAllocatedOffset > m_buffer->getSize())
	{
		if (m_currentDeletedOffset < requestedSize)
		{
			Wolf::Debug::sendCriticalError("Can't find a place in the buffer");
		}

		allocationOffset = m_currentAllocatedOffset = 0;
		m_currentDeletedOffset = 0;
	}
	r.m_bufferOffset = allocationOffset;
	r.m_bufferSize = requestedSize;

	return r;
}

void UpdateGPUBuffersPass::StagingBufferPool::deallocate(const BufferPoolInstance& bufferPoolInstance)
{
	m_currentDeletedOffset = std::max(m_currentDeletedOffset, bufferPoolInstance.m_bufferOffset + bufferPoolInstance.m_bufferSize);
	if (m_currentDeletedOffset > m_buffer->getSize())
	{
		Wolf::Debug::sendCriticalError("It shouldn't happen");
	}
}

Wolf::ResourceNonOwner<Wolf::Buffer> UpdateGPUBuffersPass::StagingBufferPool::getBuffer(const BufferPoolInstance& bufferPoolInstance)
{
	return m_buffer.createNonOwnerResource();
}
