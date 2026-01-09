#include "ComputeSkyCubeMapPass.h"

#include <Attachment.h>
#include <DescriptorSetGenerator.h>
#include <RenderMeshList.h>
#include <glm/gtc/matrix_transform.hpp>

#include "SkyBoxManager.h"

void ComputeSkyCubeMapPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

    createRenderTarget();
    createRenderPass();

    // Compute from spherical map resources
    for (uint32_t i = 0; i < 6; i++)
    {
        m_computeFromSphericalMapUniformBuffers[i].reset(new Wolf::UniformBuffer(sizeof(ComputeFromSphericalMapUniformBufferData)));
    }
    m_computeFromSphericalMapSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1.0f, VK_FILTER_LINEAR));

    m_computeFromSphericalMapDescriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::FRAGMENT, 0); // in spherical map
    m_computeFromSphericalMapDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::VERTEX, 1); // camera info
    m_computeFromSphericalMapDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_computeFromSphericalMapDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    for (uint32_t i = 0; i < 6; i++)
    {
        m_computeFromSphericalMapDescriptorSets[i].reset(Wolf::DescriptorSet::createDescriptorSet(*m_computeFromSphericalMapDescriptorSetLayout));
    }

    m_vertexShaderParser.reset(new Wolf::ShaderParser("Shaders/computeCubeMapFromSphericalMap/shader.vert"));
    m_fragmentShaderParser.reset(new Wolf::ShaderParser("Shaders/computeCubeMapFromSphericalMap/shader.frag"));

    createComputeFromSphericalMapPipeline();
}

void ComputeSkyCubeMapPass::resize(const Wolf::InitializationContext& context)
{
    // Nothing to do
}

void ComputeSkyCubeMapPass::record(const Wolf::RecordContext& context)
{
    if (!m_sphericalMapImage)
    {
        m_drawRecordedThisFrame = false;
        return;
    }

    const Wolf::ResourceUniqueOwner<Wolf::Image>& cubeMapImage = m_skyBoxManager->getCubeMapImage();

    if (m_updateComputeFromSphericalMapDescriptorSetRequested)
    {
        updateComputeFromSphericalMapDescriptorSets();
        m_updateComputeFromSphericalMapDescriptorSetRequested = false;
    }
    else
    {
        m_drawRecordedThisFrame = false;
        return;
    }

    std::vector<glm::mat4> captureViews =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    glm::mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    m_commandBuffer->beginCommandBuffer();

    cubeMapImage->transitionImageLayout(*m_commandBuffer, Wolf::Image::TransitionLayoutInfo { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
        0, 1, 0, 6, VK_IMAGE_LAYOUT_UNDEFINED });

    for (uint32_t captureViewIdx = 0; captureViewIdx < captureViews.size(); ++captureViewIdx)
    {
        ComputeFromSphericalMapUniformBufferData uniformBufferData{};
        uniformBufferData.viewProjectionMatrix = projectionMatrix * captureViews[captureViewIdx];
        m_computeFromSphericalMapUniformBuffers[captureViewIdx]->transferCPUMemory(&uniformBufferData, sizeof(uniformBufferData));

        std::vector<Wolf::ClearValue> clearValues(1);
        clearValues[0] = {{{ CLEAR_VALUE, CLEAR_VALUE, CLEAR_VALUE, 1.0f }}};
        m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffer, clearValues);

        m_commandBuffer->bindPipeline(m_computeFromSphericalMapPipeline.createConstNonOwnerResource());
        m_commandBuffer->bindDescriptorSet(m_computeFromSphericalMapDescriptorSets[captureViewIdx].createConstNonOwnerResource(), 0, *m_computeFromSphericalMapPipeline);
        m_skyBoxManager->getCubeMesh()->draw(*m_commandBuffer, Wolf::RenderMeshList::NO_CAMERA_IDX);

        m_commandBuffer->endRenderPass();

        VkImageCopy copyRegion = {};

        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.baseArrayLayer = 0;
        copyRegion.srcSubresource.mipLevel = 0;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.srcOffset = { 0, 0, 0 };

        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.baseArrayLayer = captureViewIdx;
        copyRegion.dstSubresource.mipLevel = 0;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.dstOffset = { 0, 0, 0 };

        Wolf::Extent3D extent = cubeMapImage->getExtent();
        copyRegion.extent = { extent.width, extent.height, extent.depth };

        m_renderTargetImage->setImageLayoutWithoutOperation(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        cubeMapImage->recordCopyGPUImage(*m_renderTargetImage, copyRegion, *m_commandBuffer);
        m_renderTargetImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0, 1, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL});
    }

    cubeMapImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
        0, 1, 0, 6, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL });

    m_commandBuffer->endCommandBuffer();

    m_drawRecordedThisFrame = true;
}

void ComputeSkyCubeMapPass::submit(const Wolf::SubmitContext& context)
{
    if (m_drawRecordedThisFrame)
    {
        std::vector<const Wolf::Semaphore*> waitSemaphores{ };
        const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
        m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
    }
}

void ComputeSkyCubeMapPass::clear()
{
    m_sphericalMapImage.release();
}

void ComputeSkyCubeMapPass::setInputSphericalMap(const Wolf::ResourceNonOwner<Wolf::Image>& sphericalMap)
{
    m_sphericalMapImage = sphericalMap;
    m_updateComputeFromSphericalMapDescriptorSetRequested = true;
}

void ComputeSkyCubeMapPass::onCubeMapChanged()
{
    createRenderTarget();
    createRenderPass();
    createComputeFromSphericalMapPipeline();
}

void ComputeSkyCubeMapPass::createRenderTarget()
{
    Wolf::ResourceUniqueOwner<Wolf::Image>& cubeMapImage = m_skyBoxManager->getCubeMapImage();

    Wolf::Extent3D cubMapExtent = cubeMapImage->getExtent();

    Wolf::CreateImageInfo createImageInfo{};
    createImageInfo.extent = cubMapExtent;
    createImageInfo.usage = Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT | Wolf::ImageUsageFlagBits::TRANSFER_SRC;
    createImageInfo.format = Wolf::Format::R32G32B32A32_SFLOAT;
    createImageInfo.mipLevelCount = 1;
    m_renderTargetImage.reset(Wolf::Image::createImage(createImageInfo));
}

void ComputeSkyCubeMapPass::createRenderPass()
{
    Wolf::ResourceUniqueOwner<Wolf::Image>& cubeMapImage = m_skyBoxManager->getCubeMapImage();

    Wolf::Extent3D cubMapExtent = cubeMapImage->getExtent();

    Wolf::Attachment color =  Wolf::Attachment({ cubMapExtent.width, cubMapExtent.height }, cubeMapImage->getFormat(), Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
           Wolf::AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT, m_renderTargetImage->getDefaultImageView());

    m_renderPass.reset(Wolf::RenderPass::createRenderPass({ color }));
    m_frameBuffer.reset(Wolf::FrameBuffer::createFrameBuffer(*m_renderPass, { color }));
}

void ComputeSkyCubeMapPass::updateComputeFromSphericalMapDescriptorSets()
{
    for (uint32_t captureViewIdx = 0; captureViewIdx < 6; ++captureViewIdx)
    {
        Wolf::DescriptorSetGenerator descriptorSetGenerator(m_computeFromSphericalMapDescriptorSetLayoutGenerator.getDescriptorLayouts());
        descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sphericalMapImage->getDefaultImageView(), *m_computeFromSphericalMapSampler);
        descriptorSetGenerator.setUniformBuffer(1, *m_computeFromSphericalMapUniformBuffers[captureViewIdx]);

        m_computeFromSphericalMapDescriptorSets[captureViewIdx]->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
    }
}

void ComputeSkyCubeMapPass::createComputeFromSphericalMapPipeline()
{
    Wolf::RenderingPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.renderPass = &*m_renderPass;

    // Programming stages
    pipelineCreateInfo.shaderCreateInfos.resize(2);
    m_vertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
    pipelineCreateInfo.shaderCreateInfos[0].stage = Wolf::VERTEX;
    m_fragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
    pipelineCreateInfo.shaderCreateInfos[1].stage =Wolf:: FRAGMENT;

    // IA
    std::vector<Wolf::VertexInputAttributeDescription> attributeDescriptions;
    SkyBoxManager::VertexOnlyPosition::getAttributeDescriptions(attributeDescriptions, 0);
    pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

    std::vector<Wolf::VertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0] = {};
    SkyBoxManager::VertexOnlyPosition::getBindingDescription(bindingDescriptions[0], 0);
    pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

    // Viewport
    pipelineCreateInfo.extent = { m_renderTargetImage->getExtent().width, m_renderTargetImage->getExtent().height };

    // Color Blend
    std::vector blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };
    pipelineCreateInfo.blendModes = blendModes;

    // Rasterization
    pipelineCreateInfo.cullModeFlags = Wolf::CullModeFlagBits::NONE;

    // Depth testing
    pipelineCreateInfo.enableDepthTesting = false;

    // Resources
    pipelineCreateInfo.descriptorSetLayouts = { m_computeFromSphericalMapDescriptorSetLayout.createConstNonOwnerResource() };

    m_computeFromSphericalMapPipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));
}

