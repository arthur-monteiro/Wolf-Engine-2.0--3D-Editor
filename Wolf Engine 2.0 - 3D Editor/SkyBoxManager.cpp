#include "SkyBoxManager.h"

#include <GraphicCameraInterface.h>
#include <ProfilerCommon.h>
#include <RenderPass.h>

#include "CommonLayouts.h"
#include "ComputeSkyCubeMapPass.h"

SkyBoxManager::SkyBoxManager()
{
    const std::vector<VertexOnlyPosition> cubeVertices =
    {
        // Top
        { glm::vec3(-0.5, 0.5f, -0.5f) }, // back left
        { glm::vec3(0.5f, 0.5f, -0.5f) }, // back right
        { glm::vec3(-0.5f, 0.5f, 0.5f) }, // front left
        { glm::vec3(0.5f, 0.5f, 0.5f) }, // front right

        // Bot
        { glm::vec3(-0.5, -0.5f, -0.5f) }, // back left
        { glm::vec3(0.5f, -0.5f, -0.5f) }, // back right
        { glm::vec3(-0.5f, -0.5f, 0.5f) }, // front left
        { glm::vec3(0.5f, -0.5f, 0.5f) } // front right
    };

    const std::vector<uint32_t> cubeIndices =
    {
        // Top
        0, 1, 2,
        1, 3, 2,

        // Back
        0, 4, 1,
        1, 4, 5,

        // Left
        4, 2, 0,
        4, 6, 2,

        // Right
        5, 3, 1,
        5, 7, 3,

        // Front
        2, 6, 7,
        2, 3, 7,

        // Bot
        4, 5, 6,
        5, 7, 6
    };

    m_cubeMesh.reset(new Wolf::Mesh(cubeVertices, cubeIndices));

    m_vertexShaderParser.reset(new Wolf::ShaderParser("Shaders/drawSkyBox/shader.vert", {}, 1));
    m_fragmentShaderParser.reset(new Wolf::ShaderParser("Shaders/drawSkyBox/shader.frag"));

    m_descriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::FRAGMENT, 0); // in cube map
    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

    m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));

    createCubeMap();
    m_sampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1.0f, VK_FILTER_LINEAR));
    updateDescriptorSet();
}

void SkyBoxManager::updateBeforeFrame(Wolf::WolfEngine* wolfInstance, const Wolf::ResourceNonOwner<ComputeSkyCubeMapPass>& skyCubeMapPass)
{
    if (m_cubeMapResolutionChanged)
    {
        wolfInstance->waitIdle();

        createCubeMap();
        updateDescriptorSet();
        skyCubeMapPass->onCubeMapChanged();
    }
}

void SkyBoxManager::drawSkyBox(const Wolf::CommandBuffer& commandBuffer, const Wolf::RenderPass& renderPass, const Wolf::RecordContext& context)
{
    PROFILE_FUNCTION

    createPipelineIfNeeded(renderPass);

    commandBuffer.bindPipeline(m_pipeline.createConstNonOwnerResource());
    commandBuffer.bindDescriptorSet(m_descriptorSet.createConstNonOwnerResource(), 0, *m_pipeline);
    commandBuffer.bindDescriptorSet(context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN)->getDescriptorSet(), 1, *m_pipeline);
    m_cubeMesh->draw(commandBuffer, CommonCameraIndices::CAMERA_IDX_MAIN);
}

void SkyBoxManager::setCubeMapResolution(uint32_t resolution)
{
    if (m_cubeMapResolution != resolution)
    {
        m_cubeMapResolution = resolution;
        m_cubeMapResolutionChanged = true;
    }
}

void SkyBoxManager::setOnCubeMapChangedCallback(const std::function<void(Wolf::ResourceUniqueOwner<Wolf::Image>&)>& onCubeMapChangedCallback)
{
    m_onCubeMapChangedCallback = onCubeMapChangedCallback;
}

void SkyBoxManager::createCubeMap()
{
    Wolf::CreateImageInfo createImageInfo;
    createImageInfo.extent = { m_cubeMapResolution, m_cubeMapResolution, 1 };
    createImageInfo.arrayLayerCount = 6;
    createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    createImageInfo.format = Wolf::Format::R32G32B32A32_SFLOAT;
    createImageInfo.mipLevelCount = 1;
    createImageInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
    m_cubeMapImage.reset(Wolf::Image::createImage(createImageInfo));

    std::vector<glm::vec4> initData(m_cubeMapResolution * m_cubeMapResolution, glm::vec4(CLEAR_VALUE, CLEAR_VALUE, CLEAR_VALUE, 1.0f));
    for (uint32_t i = 0; i < 6; ++i)
    {
        m_cubeMapImage->copyCPUBuffer(reinterpret_cast<unsigned char*>(initData.data()), { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
           0, 1, i, 1 }, 0, i);
    }
}

void SkyBoxManager::createPipelineIfNeeded(const Wolf::RenderPass& renderPass)
{
    Wolf::Extent2D extent = { renderPass.getExtent().width, renderPass.getExtent().height };

    if (m_pipeline && m_lastPipelineRenderTargetExtent == extent)
        return;

    m_lastPipelineRenderTargetExtent = extent;

    Wolf::RenderingPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.renderPass = &renderPass;

    // Programming stages
    pipelineCreateInfo.shaderCreateInfos.resize(2);
    m_vertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
    pipelineCreateInfo.shaderCreateInfos[0].stage = Wolf::VERTEX;
    m_fragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
    pipelineCreateInfo.shaderCreateInfos[1].stage = Wolf::FRAGMENT;

    // IA
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    SkyBoxManager::VertexOnlyPosition::getAttributeDescriptions(attributeDescriptions, 0);
    pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0] = {};
    SkyBoxManager::VertexOnlyPosition::getBindingDescription(bindingDescriptions[0], 0);
    pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

    // Viewport
    pipelineCreateInfo.extent = extent;

    // Color Blend
    std::vector blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };
    pipelineCreateInfo.blendModes = blendModes;

    // Rasterization
    pipelineCreateInfo.cullMode = VK_CULL_MODE_NONE;

    // Depth testing
    pipelineCreateInfo.enableDepthTesting = false;

    // Resources
    pipelineCreateInfo.descriptorSetLayouts = { m_descriptorSetLayout.createConstNonOwnerResource(), Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource() };

    // Dynamic state
    pipelineCreateInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

    m_pipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));
}

void SkyBoxManager::updateDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_cubeMapImage->getDefaultImageView(), *m_sampler);

    m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}
