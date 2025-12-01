#include "UpdateRayTracedWorldPass.h"

UpdateRayTracedWorldPass::UpdateRayTracedWorldPass(const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager) : m_rayTracedWorldManager(rayTracedWorldManager)
{
}

void UpdateRayTracedWorldPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::COMPUTE, false));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);
}

void UpdateRayTracedWorldPass::resize(const Wolf::InitializationContext& context)
{
    // Nothing to do
}

void UpdateRayTracedWorldPass::record(const Wolf::RecordContext& context)
{
    m_wasEnabledThisFrame = m_rayTracedWorldManager->needsRebuildTLAS();

    if (!m_wasEnabledThisFrame)
        return;

    m_commandBuffer->beginCommandBuffer();

    m_rayTracedWorldManager->build(*m_commandBuffer);

    m_commandBuffer->endCommandBuffer();
}

void UpdateRayTracedWorldPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_wasEnabledThisFrame)
        return;

    std::vector<const Wolf::Semaphore*> waitSemaphores{ };
    const std::vector<const Wolf::Semaphore*> signalSemaphores{ CommandRecordBase::getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);
}
