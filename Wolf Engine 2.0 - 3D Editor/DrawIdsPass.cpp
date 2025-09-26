#include "DrawIdsPass.h"

#include <DebugMarker.h>
#include <ProfilerCommon.h>
#include <RenderMeshList.h>

void DrawIdsPass::initializeResources(const Wolf::InitializationContext& context)
{
    Wolf::Attachment color = setupColorAttachment(context);
    Wolf::Attachment depth = setupDepthAttachment(context);

    m_renderPass.reset(Wolf::RenderPass::createRenderPass({ color, depth }));
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false /* isTransient */));
    initializeFramesBuffer(context, color, depth);

    createSemaphores(context, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, true);
}

void DrawIdsPass::resize(const Wolf::InitializationContext& context)
{
    m_renderPass->setExtent({ context.swapChainWidth, context.swapChainHeight });

    Wolf::Attachment color = setupColorAttachment(context);
    Wolf::Attachment depth = setupDepthAttachment(context);
    initializeFramesBuffer(context, color, depth);
}

void DrawIdsPass::record(const Wolf::RecordContext& context)
{
    PROFILE_FUNCTION

    m_lastRecordFrameNumber = Wolf::g_runtimeContext->getCurrentCPUFrameNumber();

    if (m_requestFrameNumber != m_lastRecordFrameNumber && m_requestFrameNumber != NO_REQUEST)
    {
        if (m_lastRecordFrameNumber == m_requestFrameNumber + Wolf::g_configuration->getMaxCachedFrames())
        {
            VkSubresourceLayout imageResourceLayout;
            m_copyImage->getResourceLayout(imageResourceLayout);
            uint32_t* mappedImage = static_cast<uint32_t*>(m_copyImage->map());
            uint32_t offset = m_requestPixelPosX + m_requestPixelPosY * (imageResourceLayout.rowPitch / sizeof(uint32_t));
            uint32_t pixel = mappedImage[offset];
            m_copyImage->unmap();

            m_requestPixelCallback(pixel);
            m_requestFrameNumber = NO_REQUEST;
        }
        else
        {
            m_recordCommandsThisFrame = false;
        }
        return;
    }

    m_recordCommandsThisFrame = m_recordCommandsNextFrame;
    m_recordCommandsNextFrame = false;

    if (!m_recordCommandsThisFrame)
        return;

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Draw Ids Pass");

    std::vector<Wolf::ClearValue> clearValues(1);
    clearValues[0].color.uint32[0] = static_cast<uint32_t>(-1);
    m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffer, clearValues);

    const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();
    m_commandBuffer->setViewport(renderViewport);

    context.renderMeshList->draw(context, *m_commandBuffer, &*m_renderPass, CommonPipelineIndices::PIPELINE_IDX_OUTPUT_IDS, CommonCameraIndices::CAMERA_IDX_MAIN, {});

    m_commandBuffer->endRenderPass();
    m_outputImage->setImageLayoutWithoutOperation(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImageCopy copyRegion = {};

    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.srcOffset = { 0, 0, 0 };

    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.dstOffset = { 0, 0, 0 };

    Wolf::Extent3D extent = m_copyImage->getExtent();
    copyRegion.extent = { extent.width, extent.height, extent.depth };

    m_copyImage->recordCopyGPUImage(*m_outputImage, copyRegion, *m_commandBuffer);
    m_outputImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL });

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();
}

void DrawIdsPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_recordCommandsThisFrame)
        return;

    std::vector<const Wolf::Semaphore*> waitSemaphores { m_forwardPass->getSemaphore(context.swapChainImageIndex) };

    const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
}

void DrawIdsPass::requestPixelBeforeFrame(uint32_t posX, uint32_t posY, const PixelRequestCallback& callback)
{
    if (m_requestFrameNumber != NO_REQUEST)
    {
        Wolf::Debug::sendError("Can't have multiple request at the same time");
        return;
    }

    m_requestPixelPosX = posX;
    m_requestPixelPosY = posY;
    m_requestPixelCallback = callback;
    m_recordCommandsNextFrame = true;
    m_requestFrameNumber = Wolf::g_runtimeContext->getCurrentCPUFrameNumber();
}

Wolf::Attachment DrawIdsPass::setupColorAttachment(const Wolf::InitializationContext& context)
{
    Wolf::CreateImageInfo createRenderTargetInfo;
    createRenderTargetInfo.extent = { context.swapChainWidth, context.swapChainHeight, 1 };
    createRenderTargetInfo.usage = Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT | Wolf::ImageUsageFlagBits::TRANSFER_SRC;
    createRenderTargetInfo.format = OUTPUT_FORMAT;
    createRenderTargetInfo.mipLevelCount = 1;
    m_outputImage.reset(Wolf::Image::createImage(createRenderTargetInfo));

    Wolf::CreateImageInfo createCopyInfo = createRenderTargetInfo;
    createCopyInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST;
    createCopyInfo.imageTiling = VK_IMAGE_TILING_LINEAR;
    createCopyInfo.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    m_copyImage.reset(Wolf::Image::createImage(createCopyInfo));
    m_copyImage->setImageLayout({ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED });

    return Wolf::Attachment({ context.swapChainWidth, context.swapChainHeight }, OUTPUT_FORMAT, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Wolf::AttachmentStoreOp::STORE,
        Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT, m_outputImage->getDefaultImageView());
}

Wolf::Attachment DrawIdsPass::setupDepthAttachment(const Wolf::InitializationContext& context)
{
    Wolf::Attachment attachment({ context.swapChainWidth, context.swapChainHeight }, context.depthFormat, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        Wolf::AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT, m_preDepthPass->getOutput()->getDefaultImageView());
    attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment.loadOperation = Wolf::AttachmentLoadOp::LOAD;

    return attachment;
}

void DrawIdsPass::initializeFramesBuffer(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment)
{
    m_frameBuffer.reset(Wolf::FrameBuffer::createFrameBuffer(*m_renderPass, { colorAttachment,  depthAttachment }));
}
