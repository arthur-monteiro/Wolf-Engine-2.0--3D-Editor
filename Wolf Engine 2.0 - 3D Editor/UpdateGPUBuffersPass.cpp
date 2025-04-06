#include "UpdateGPUBuffersPass.h"

#include <Configuration.h>
#include <DebugMarker.h>
#include <ProfilerCommon.h>
#include <RuntimeContext.h>

#include <PushDataToGPU.h>

UpdateGPUBuffersPass* g_updateGPUBufferPassInst = nullptr;

void Wolf::pushDataToGPUBuffer(const void* data, uint32_t size, const ResourceNonOwner<Buffer>& outputBuffer, uint32_t outputOffset)
{
	g_updateGPUBufferPassInst->addRequestBeforeFrame({ data, size, outputBuffer, outputOffset });
}

void Wolf::pushDataToGPUImage(const unsigned char* pixels, const ResourceNonOwner<Image>& outputImage, const Image::TransitionLayoutInfo& finalLayout, uint32_t mipLevel)
{
	g_updateGPUBufferPassInst->addRequestBeforeFrame({ pixels, outputImage, finalLayout, mipLevel });
}

UpdateGPUBuffersPass::UpdateGPUBuffersPass()
{
	g_updateGPUBufferPassInst = this;
}

void UpdateGPUBuffersPass::initializeResources(const Wolf::InitializationContext& context)
{
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::TRANSFER, false));
	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

	m_currentRequestsQueues.resize(Wolf::g_configuration->getMaxCachedFrames());
}

void UpdateGPUBuffersPass::resize(const Wolf::InitializationContext& context)
{
	// Nothing to do
}

void UpdateGPUBuffersPass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	uint32_t requestQueueIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();

	m_currentRequestsQueues[requestQueueIdx].swap(m_addRequestsQueue);
	m_addRequestsQueue.clear();

	if (m_currentRequestsQueues.empty())
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
		const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };
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

void UpdateGPUBuffersPass::InternalRequest::recordCopyToBuffer(const Wolf::CommandBuffer* commandBuffer) const
{
	Wolf::Buffer::BufferCopy bufferCopy;
	bufferCopy.srcOffset = 0;
	bufferCopy.dstOffset = m_request.getOutputOffset();
	bufferCopy.size = m_request.getSize();

	m_request.getOutputBuffer()->recordTransferGPUMemory(commandBuffer, *m_stagingBuffer, bufferCopy);
}

void UpdateGPUBuffersPass::InternalRequest::recordCopyToImage(const Wolf::CommandBuffer* commandBuffer) const
{
	const Wolf::Extent3D extent = { m_request.getOutputImage()->getExtent().width >> m_request.getMipLevel(), m_request.getOutputImage()->getExtent().height >> m_request.getMipLevel(), m_request.getOutputImage()->getExtent().depth };

	Wolf::Image::BufferImageCopy bufferImageCopy;
	bufferImageCopy.bufferOffset = 0;
	bufferImageCopy.bufferRowLength = extent.width;
	bufferImageCopy.bufferImageHeight = extent.height;

	bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferImageCopy.imageSubresource.mipLevel = m_request.getMipLevel();
	bufferImageCopy.imageSubresource.baseArrayLayer = 0;
	bufferImageCopy.imageSubresource.layerCount = 1;

	bufferImageCopy.imageOffset = { 0, 0, 0 };
	bufferImageCopy.imageExtent = extent;

	m_request.getOutputImage()->recordCopyGPUBuffer(*commandBuffer, *m_stagingBuffer, bufferImageCopy, m_request.getImageFinalLayout());
}

void UpdateGPUBuffersPass::addRequestBeforeFrame(const Request& request)
{
	m_addRequestQueueMutex.lock();
	InternalRequest* newInternalRequest = new InternalRequest(request);
	m_addRequestsQueue.emplace_back(newInternalRequest);
	m_addRequestQueueMutex.unlock();
}
