#include "CustomSceneRenderPass.h"

#include <Attachment.h>
#include <CameraList.h>
#include <DebugMarker.h>
#include <DescriptorSetGenerator.h>
#include <RenderMeshList.h>

#include "CommonLayouts.h"

void CustomSceneRenderPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);
}

void CustomSceneRenderPass::resize(const Wolf::InitializationContext& context)
{
    // Nothing to do
}

void CustomSceneRenderPass::record(const Wolf::RecordContext& context)
{
    if (m_currentRequestsQueue.empty())
    {
        m_commandsRecordedThisFrame = false;
        return;
    }

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Custom render pass");

    for (uint32_t i = 0; i < m_currentRequestsQueue.size(); ++i)
    {
        const Wolf::ResourceUniqueOwner<Request>& request = m_currentRequestsQueue[i];
        request->recordCommands(m_commandBuffer.get(), context);
    }

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_commandsRecordedThisFrame = true;
}

void CustomSceneRenderPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_commandsRecordedThisFrame)
        return;

    const std::vector<const Wolf::Semaphore*> waitSemaphores{ };
    const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
}

void CustomSceneRenderPass::updateBeforeFrame(Wolf::CameraList& cameraList)
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
        request->setCameraIdx(CommonCameraIndices::CAMERA_IDX_FIRST_CUSTOM_RENDER_PASS + i);
    }

    addCamerasForThisFrame(cameraList);
}

void CustomSceneRenderPass::addCamerasForThisFrame(Wolf::CameraList& cameraList) const
{
    DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_CONST_RANGE_LOOP(m_currentRequestsQueue, request,
        if (request->isRunning())
        {
            cameraList.addCameraForThisFrame(&*request->getCamera(), request->getCameraIdx());
        }
    )
}

void CustomSceneRenderPass::Request::recordCommands(const Wolf::CommandBuffer* commandBuffer, const Wolf::RecordContext& context)
{
    if (m_cameraIdx == -1 || !m_isRunning)
        return;

    std::vector<Wolf::ClearValue> clearValues(m_outputs.size() + 1);
    clearValues[0] = {{{1.0f}}};
    for (uint32_t i = 0; i < m_outputs.size(); ++i)
    {
        clearValues[i + 1] = {{{0.0f, 0.0f, 0.0f}}};
    }

    commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffer, clearValues);

    Wolf::DescriptorSetBindInfo descriptorSetBindInfo(m_descriptorSet.createConstNonOwnerResource(), m_descriptorSetLayout.createConstNonOwnerResource(),
        DescriptorSetSlots::DESCRIPTOR_SET_SLOT_PASS_INFO);
    std::vector<Wolf::RenderMeshList::AdditionalDescriptorSet> descriptorSetBindInfos;
    descriptorSetBindInfos.emplace_back(descriptorSetBindInfo, 0);

    std::vector<Wolf::PipelineSet::ShaderCodeToAddForStage> shadersCodeToAdd(1);
    shadersCodeToAdd[0].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

    for (uint32_t i = 0; i < m_outputs.size(); ++i)
    {
        const Output& output = m_outputs[i];
        switch (output.m_displayType)
        {
            case GameContext::DisplayType::ALBEDO:
                shadersCodeToAdd[0].shaderCodeToAdd.codeString += "#define ALBEDO\n";
                shadersCodeToAdd[0].shaderCodeToAdd.codeString += "#define ALBEDO_LOCATION " + std::to_string(i) + "\n";
                break;
            case GameContext::DisplayType::NORMAL:
                shadersCodeToAdd[0].shaderCodeToAdd.codeString += "#define NORMAL\n";
                shadersCodeToAdd[0].shaderCodeToAdd.codeString += "#define NORMAL_LOCATION " + std::to_string(i) + "\n";
                break;
            case GameContext::DisplayType::VERTEX_COLOR:
                shadersCodeToAdd[0].shaderCodeToAdd.codeString += "#define VERTEX_COLOR\n";
                shadersCodeToAdd[0].shaderCodeToAdd.codeString += "#define VERTEX_COLOR_LOCATION " + std::to_string(i) + "\n";
                break;
            case GameContext::DisplayType::VERTEX_NORMAL:
                shadersCodeToAdd[0].shaderCodeToAdd.codeString += "#define VERTEX_NORMAL\n";
                shadersCodeToAdd[0].shaderCodeToAdd.codeString += "#define VERTEX_NORMAL_LOCATION " + std::to_string(i) + "\n";
                break;
            case GameContext::DisplayType::ROUGHNESS:
            case GameContext::DisplayType::METALNESS:
            case GameContext::DisplayType::MAT_AO:
            case GameContext::DisplayType::ANISO_STRENGTH:
            case GameContext::DisplayType::LIGHTING:
            case GameContext::DisplayType::ENTITY_IDX:
            case GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_ALBEDO:
            case GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_INSTANCE_ID:
            case GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_PRIMITIVE_ID:
            case GameContext::DisplayType::PATH_TRACING:
            case GameContext::DisplayType::GLOBAL_IRRADIANCE:
            default:
                Wolf::Debug::sendError("Unsupported display type for custom render: " + std::to_string(static_cast<uint32_t>(output.m_displayType)));
        }
    }

    context.renderMeshList->draw(context, *commandBuffer, &*m_renderPass, CommonPipelineIndices::PIPELINE_IDX_CUSTOM_RENDER, m_cameraIdx, descriptorSetBindInfos, shadersCodeToAdd);

    commandBuffer->endRenderPass();
}

void CustomSceneRenderPass::Request::initResources()
{
    std::vector<Wolf::Attachment> attachments;
    attachments.emplace_back(Wolf::Extent2D{ m_outputDepth->getExtent().width, m_outputDepth->getExtent().height }, m_outputDepth->getFormat(), Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_GENERAL,
        Wolf::AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT, m_outputDepth->getDefaultImageView());

    for (const Output& output : m_outputs)
    {
        attachments.emplace_back(Wolf::Extent2D{ output.m_image->getExtent().width, output.m_image->getExtent().height }, output.m_image->getFormat(), Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_GENERAL,
            Wolf::AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT, output.m_image->getDefaultImageView());
    }

    m_renderPass.reset(Wolf::RenderPass::createRenderPass(attachments));
    m_frameBuffer.reset(Wolf::FrameBuffer::createFrameBuffer(*m_renderPass, attachments));

    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));

    m_descriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setUniformBuffer(0, *m_uniformBuffer);

    m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
    m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

CustomSceneRenderPass::RequestId CustomSceneRenderPass::addRequestBeforeFrame(Request& request)
{
    m_addRequestQueueMutex.lock();

    if (m_currentRequestsQueue.size() + m_addRequestsQueue.size() >= MAX_CUSTOM_RENDER_REQUESTS_COUNT)
    {
        m_addRequestQueueMutex.unlock();
        Wolf::Debug::sendError("Maximum number of requests reached");
        return static_cast<uint32_t>(-1);
    }

    m_addRequestsQueue.emplace_back(new Request(request));
    uint32_t returnValue = static_cast<uint32_t>(m_currentRequestsQueue.size() + m_addRequestsQueue.size() - 1);

    m_addRequestQueueMutex.unlock();

    return returnValue;
}

void CustomSceneRenderPass::updateRequestBeforeFrame(RequestId requestIdx, Request& request)
{
    m_currentRequestsQueue[requestIdx] = Wolf::ResourceUniqueOwner<Request>(new Request(request));
}

bool CustomSceneRenderPass::isRequestRunning(RequestId requestIdx) const
{
    return m_currentRequestsQueue[requestIdx]->isRunning();
}

void CustomSceneRenderPass::pauseRequestBeforeFrame(RequestId requestIdx)
{
    m_currentRequestsQueue[requestIdx]->pause();
}

void CustomSceneRenderPass::unpauseRequestBeforeFrame(RequestId requestIdx)
{
    m_currentRequestsQueue[requestIdx]->unpause();
}
