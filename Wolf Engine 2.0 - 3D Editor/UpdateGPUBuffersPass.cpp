#include "UpdateGPUBuffersPass.h"

#include <DebugMarker.h>
#include <ProfilerCommon.h>

void UpdateGPUBuffersPass::initializeResources(const Wolf::InitializationContext& context)
{
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::TRANSFER, false));
	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));
}

void UpdateGPUBuffersPass::resize(const Wolf::InitializationContext& context)
{
	// Nothing to do
}

void UpdateGPUBuffersPass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	m_currentRequestsQueue.swap(m_addRequestsQueue);
	m_addRequestsQueue.clear();

	if (m_currentRequestsQueue.empty())
	{
		m_transferRecordedThisFrame = false;
		return;
	}

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Update GPU buffers pass");

	for (const InternalRequest& request : m_currentRequestsQueue)
	{
		request.recordCopyToBuffer(m_commandBuffer.get());
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
	m_currentRequestsQueue.clear();
}

void UpdateGPUBuffersPass::InternalRequest::recordCopyToBuffer(Wolf::CommandBuffer* commandBuffer) const
{
	Wolf::Buffer::BufferCopy bufferCopy;
	bufferCopy.srcOffset = 0;
	bufferCopy.dstOffset = m_request.getOutputOffset();
	bufferCopy.size = m_request.getSize();

	m_request.getOutputBuffer()->recordTransferGPUMemory(commandBuffer, *m_stagingBuffer, bufferCopy);
}

void UpdateGPUBuffersPass::addRequestBeforeFrame(const Request& request)
{
	m_addRequestsQueue.emplace_back(request);
}
