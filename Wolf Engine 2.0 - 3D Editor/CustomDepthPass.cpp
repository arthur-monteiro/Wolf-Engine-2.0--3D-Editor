#include "CustomDepthPass.h"

#include <Attachment.h>
#include <DebugMarker.h>
#include <RenderMeshList.h>

#include "CameraList.h"
#include "CommonLayouts.h"

void CustomDepthPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);
}

void CustomDepthPass::resize(const Wolf::InitializationContext& context)
{
    // Nothing to do
}

void CustomDepthPass::record(const Wolf::RecordContext& context)
{
    if (m_currentRequestsQueue.empty())
    {
        m_commandsRecordedThisFrame = false;
        return;
    }

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Custom depth pass");

    for (uint32_t i = 0; i < m_currentRequestsQueue.size(); ++i)
    {
        const Wolf::ResourceUniqueOwner<Request>& request = m_currentRequestsQueue[i];
        request->recordCommands(m_commandBuffer.get(), context);
    }

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_commandsRecordedThisFrame = true;
}

void CustomDepthPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_commandsRecordedThisFrame)
        return;

    const std::vector<const Wolf::Semaphore*> waitSemaphores{ };
    const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
}

void CustomDepthPass::updateBeforeFrame(Wolf::CameraList& cameraList)
{
    if (!m_addRequestsQueue.empty())
    {
        m_addRequestQueueMutex.lock();
        DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_addRequestsQueue, request,
            {
                m_currentRequestsQueue.emplace_back(request.release());
            }
        )
        m_addRequestsQueue.clear();
        m_addRequestQueueMutex.unlock();
    }

    for (uint32_t i = 0; i < m_currentRequestsQueue.size(); ++i)
    {
        Wolf::ResourceUniqueOwner<Request>& request = m_currentRequestsQueue[i];
        request->setCameraIdx(CommonCameraIndices::CAMERA_IDX_FIRST_CUSTOM_DEPTH_PASS + i);
    }

    addCamerasForThisFrame(cameraList);
}

void CustomDepthPass::addCamerasForThisFrame(Wolf::CameraList& cameraList) const
{
    DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_CONST_RANGE_LOOP(m_currentRequestsQueue, request,
        cameraList.addCameraForThisFrame(&*request->getCamera(), request->getCameraIdx());
    )
}

void CustomDepthPass::Request::recordCommands(const Wolf::CommandBuffer* commandBuffer, const Wolf::RecordContext& context)
{
    if (m_cameraIdx == -1)
        return;

    std::vector<Wolf::ClearValue> clearValues(1);
    clearValues[0] = {{{1.0f}}};
    commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffer, clearValues);

    context.renderMeshList->draw(context, *commandBuffer, &*m_renderPass, CommonPipelineIndices::PIPELINE_IDX_CUSTOM_DEPTH, m_cameraIdx, {}, {});

    commandBuffer->endRenderPass();
}

void CustomDepthPass::Request::initResources()
{
    std::vector<Wolf::Attachment> attachments;
    attachments.emplace_back(Wolf::Extent2D{ m_outputDepth->getExtent().width, m_outputDepth->getExtent().height }, m_outputDepth->getFormat(), Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_GENERAL,
        Wolf::AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT, m_outputDepth->getDefaultImageView());

    m_renderPass.reset(Wolf::RenderPass::createRenderPass(attachments));
    m_frameBuffer.reset(Wolf::FrameBuffer::createFrameBuffer(*m_renderPass, attachments));
}

void CustomDepthPass::addRequestBeforeFrame(Request& request)
{
    m_addRequestQueueMutex.lock();

    if (m_currentRequestsQueue.size() + m_addRequestsQueue.size() >= MAX_CUSTOM_DEPTH_REQUESTS_COUNT)
    {
        m_addRequestQueueMutex.unlock();
        Wolf::Debug::sendError("Maximum number of requests reached");
        return;
    }

    m_addRequestsQueue.emplace_back(new Request(request));
    m_addRequestQueueMutex.unlock();
}
