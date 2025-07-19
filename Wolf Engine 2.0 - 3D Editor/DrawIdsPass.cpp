#include "DrawIdsPass.h"

void DrawIdsPass::initializeResources(const Wolf::InitializationContext& context)
{
    Wolf::Attachment color = setupColorAttachment(context);
    Wolf::Attachment depth = setupDepthAttachment(context);

    //m_renderPass.reset(Wolf::RenderPass::createRenderPass({ color, depth }));
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false /* isTransient */));
    //initializeFramesBuffers(context, color, depth);

    m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT));

}

Wolf::Attachment DrawIdsPass::setupColorAttachment(const Wolf::InitializationContext& context)
{
    return Wolf::Attachment({ context.swapChainWidth, context.swapChainHeight }, OUTPUT_FORMAT, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Wolf::AttachmentStoreOp::STORE,
        Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT, nullptr);
}
