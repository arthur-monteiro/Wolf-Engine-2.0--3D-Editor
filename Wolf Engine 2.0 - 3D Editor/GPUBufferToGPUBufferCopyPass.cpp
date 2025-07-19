#include "GPUBufferToGPUBufferCopyPass.h"

#include <Configuration.h>
#include <ProfilerCommon.h>
#include <RuntimeContext.h>

#include <DebugMarker.h>
#include <ReadbackDataFromGPU.h>

GPUBufferToGPUBufferCopyPass* g_GPUBufferToGPUBufferCopyPassInst = nullptr;

void Wolf::requestGPUBufferReadbackRecord(const ResourceNonOwner<Buffer>& srcBuffer, uint32_t srcOffset, const ResourceNonOwner<ReadableBuffer>& readableBuffer, uint32_t size)
{
    g_GPUBufferToGPUBufferCopyPassInst->addRequestBeforeFrame({ srcBuffer, srcOffset, readableBuffer, size });
}

GPUBufferToGPUBufferCopyPass::GPUBufferToGPUBufferCopyPass(const Wolf::ResourceNonOwner<const ForwardPass>& forwardPass) : m_forwardPass(forwardPass)
{
    g_GPUBufferToGPUBufferCopyPassInst = this;
}

void GPUBufferToGPUBufferCopyPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::TRANSFER, false));
    m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

    m_currentRequestsQueues.resize(Wolf::g_configuration->getMaxCachedFrames());
}

void GPUBufferToGPUBufferCopyPass::resize(const Wolf::InitializationContext& context)
{
    // Nothing to do
}

void GPUBufferToGPUBufferCopyPass::record(const Wolf::RecordContext& context)
{
    PROFILE_FUNCTION

    uint32_t requestQueueIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();

    m_currentRequestsQueues[requestQueueIdx].swap(m_addRequestsQueue);
    m_addRequestsQueue.clear();

    if (m_currentRequestsQueues[requestQueueIdx].empty())
    {
        m_commandsRecordedThisFrame = false;
        return;
    }

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "GPU buffer to GPU buffer copy pass");

    for (uint32_t i = 0; i < m_currentRequestsQueues[requestQueueIdx].size(); ++i)
    {
        const Wolf::ResourceUniqueOwner<Request>& request = m_currentRequestsQueues[requestQueueIdx][i];
        request->recordCommands(m_commandBuffer.get());
    }

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_commandsRecordedThisFrame = true;
}

void GPUBufferToGPUBufferCopyPass::submit(const Wolf::SubmitContext& context)
{
    if (m_commandsRecordedThisFrame)
    {
        std::vector<const Wolf::Semaphore*> waitSemaphores{ m_forwardPass->getSemaphore() };
        const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };
        m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
    }
}

bool GPUBufferToGPUBufferCopyPass::isCurrentQueueEmpty() const
{
    return m_addRequestsQueue.empty();
}

void GPUBufferToGPUBufferCopyPass::Request::recordCommands(const Wolf::CommandBuffer* commandBuffer) const
{
    uint32_t dstBufferIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();

    Wolf::Buffer::BufferCopy bufferCopy = {};
    bufferCopy.srcOffset = m_srcOffset;
    bufferCopy.dstOffset = 0;
    bufferCopy.size = m_size;

    m_readableBuffer->getBuffer(dstBufferIdx).recordTransferGPUMemory(commandBuffer, *m_srcBuffer, bufferCopy);
}

void GPUBufferToGPUBufferCopyPass::addRequestBeforeFrame(const Request& request)
{
    m_addRequestQueueMutex.lock();
    Request* newRequest = new Request(request);
    m_addRequestsQueue.emplace_back(newRequest);
    m_addRequestQueueMutex.unlock();
}
