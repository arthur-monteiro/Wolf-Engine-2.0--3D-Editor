#include "RayTracedShadowsPass.h"

#include <CameraList.h>
#include <DebugMarker.h>
#include <fstream>
#include <GraphicCameraInterface.h>
#include <MaterialsGPUManager.h>
#include <Pipeline.h>
#include <RayTracingShaderGroupGenerator.h>
#include <ShaderBindingTable.h>

#include "GameContext.h"
#include "LightManager.h"

struct GameContext;

RayTracedShadowsPass::RayTracedShadowsPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<UpdateRayTracedWorldPass>& updateRayTracedWorldPass,
        const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager)
    : m_editorParams(editorParams), m_preDepthPass(preDepthPass), m_updateRayTracedWorldPass(updateRayTracedWorldPass), m_rayTracedWorldManager(rayTracedWorldManager)
{
}

void RayTracedShadowsPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::RAY_TRACING, false /* isTransient */));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

    Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
    m_rayTracedWorldManager->addRayGenShaderCode(shaderCodeToAdd, 2);
    m_rayGenShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedShadows/shader.rgen", {}, 1, -1, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_rayMissShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedShadows/shader.rmiss", {}, -1, -1, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_closestHitShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedShadows/shader.rchit", {}, -1, -1, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));

    m_rayTracingDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::RAYGEN, 0); // output image
    m_rayTracingDescriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, Wolf::ShaderStageFlagBits::RAYGEN, 1, 1); // depth image
    m_rayTracingDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 2); // uniform buffer
    m_rayTracingDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    m_outputMaskDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
    m_outputMaskDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_outputMaskDescriptorSetLayoutGenerator.getDescriptorLayouts()));
    m_outputMaskDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_outputMaskDescriptorSetLayout));

    createOutputImage(context);
    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));
    createPipeline();
    createDescriptorSet();
}

void RayTracedShadowsPass::resize(const Wolf::InitializationContext& context)
{
    createOutputImage(context);
    createDescriptorSet();
}

void RayTracedShadowsPass::record(const Wolf::RecordContext& context)
{
    const GameContext* gameContext = static_cast<const GameContext*>(context.gameContext);
    if (!m_rayTracedWorldManager->hasInstance() || gameContext->displayType != GameContext::DisplayType::LIGHTING)
    {
        m_wasEnabledThisFrame = false;
        return;
    }

    uint32_t sunLightCount = context.lightManager->getSunLightCount();
    if (sunLightCount == 0)
    {
        return;
    }

    glm::vec3 sunDirection = context.lightManager->getSunLightInfo(0).direction;

    UniformBufferData uniformBufferData{};
    uniformBufferData.sunDirectionAndNoiseIndex = glm::vec4(sunDirection, 0.0f);
    uniformBufferData.screenOffsetX = m_editorParams->getRenderOffsetLeft();
    uniformBufferData.screenOffsetY = m_editorParams->getRenderOffsetTop();
    m_uniformBuffer->transferCPUMemory(&uniformBufferData, sizeof(uniformBufferData), 0);

    const Wolf::CameraInterface* camera = context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN);

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::rayTracePassDebugColor, "Ray Traced Shadows");

    m_commandBuffer->bindPipeline(m_pipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_rayTracingDescriptorSet.createConstNonOwnerResource(), 0, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(camera->getDescriptorSet(), 1, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(m_rayTracedWorldManager->getDescriptorSet(), 2, *m_pipeline);

    m_commandBuffer->traceRays(m_shaderBindingTable.createConstNonOwnerResource(), { m_editorParams->getRenderWidth(), m_editorParams->getRenderHeight(), 1 });

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_wasEnabledThisFrame = true;
}

void RayTracedShadowsPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_wasEnabledThisFrame)
        return;

    std::vector<const Wolf::Semaphore*> waitSemaphores{ m_preDepthPass->getSemaphore(context.swapChainImageIndex) };
    if (m_updateRayTracedWorldPass->wasEnabledThisFrame())
        waitSemaphores.push_back(m_updateRayTracedWorldPass->getSemaphore(context.swapChainImageIndex));

    const std::vector<const Wolf::Semaphore*> signalSemaphores{ CommandRecordBase::getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);

    bool anyShaderModified = m_rayGenShaderParser->compileIfFileHasBeenModified();
    if (m_rayMissShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;
    if (m_closestHitShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;

    if (anyShaderModified)
    {
        context.graphicAPIManager->waitIdle();
        createPipeline();
    }
}

void RayTracedShadowsPass::addComputeShadowsShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const
{
    std::ifstream inFile("Shaders/rayTracedShadows/computeShadows.glsl");
    std::string line;
    while (std::getline(inFile, line))
    {
        inOutShaderCodeToAdd.codeString += line + '\n';
    }
}

void RayTracedShadowsPass::createOutputImage(const Wolf::InitializationContext& context)
{
    Wolf::CreateImageInfo createInfo{};
    createInfo.extent = { context.swapChainWidth, context.swapChainHeight, 1 };
    createInfo.usage = Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::STORAGE;
    createInfo.format = Wolf::Format::R16_SFLOAT;
    createInfo.mipLevelCount = 1;
    m_outputImage.reset(Wolf::Image::createImage(createInfo));
    m_outputImage->setImageLayout({ Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, 0, 1,
        Wolf::ImageLayout::UNDEFINED });
}

void RayTracedShadowsPass::createPipeline()
{
    Wolf::RayTracingShaderGroupGenerator shaderGroupGenerator;
    shaderGroupGenerator.addRayGenShaderStage(0);
    shaderGroupGenerator.addMissShaderStage(1);
    Wolf::HitGroup hitGroup;
    hitGroup.closestHitShaderIdx = 2;
    shaderGroupGenerator.addHitGroup(hitGroup);

    Wolf::RayTracingPipelineCreateInfo pipelineCreateInfo;

    std::vector<char> rayGenShaderCode;
    m_rayGenShaderParser->readCompiledShader(rayGenShaderCode);
    std::vector<char> rayMissShaderCode;
    m_rayMissShaderParser->readCompiledShader(rayMissShaderCode);
    std::vector<char> closestHitShaderCode;
    m_closestHitShaderParser->readCompiledShader(closestHitShaderCode);

    std::vector<Wolf::ShaderCreateInfo> shaders(3);
    shaders[0].shaderCode = rayGenShaderCode;
    shaders[0].stage = Wolf::ShaderStageFlagBits::RAYGEN;
    shaders[1].shaderCode = rayMissShaderCode;
    shaders[1].stage = Wolf::ShaderStageFlagBits::MISS;
    shaders[2].shaderCode = closestHitShaderCode;
    shaders[2].stage = Wolf::ShaderStageFlagBits::CLOSEST_HIT;
    pipelineCreateInfo.shaderCreateInfos = shaders;

    pipelineCreateInfo.shaderGroupsCreateInfos = shaderGroupGenerator.getShaderGroups();

    std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts =
    {
        m_rayTracingDescriptorSetLayout.createConstNonOwnerResource(),
        Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource(),
        RayTracedWorldManager::getDescriptorSetLayout().createConstNonOwnerResource()
    };
    m_pipeline.reset(Wolf::Pipeline::createRayTracingPipeline(pipelineCreateInfo, descriptorSetLayouts));

    m_shaderBindingTable.reset(new Wolf::ShaderBindingTable(static_cast<uint32_t>(shaders.size()), *m_pipeline));
}

void RayTracedShadowsPass::createDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts());
    const Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc(Wolf::ImageLayout::GENERAL, m_outputImage->getDefaultImageView());
    descriptorSetGenerator.setImage(0, outputImageDesc);

    const Wolf::DescriptorSetGenerator::ImageDescription depthImageDesc(Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL, m_preDepthPass->getOutput()->getDefaultImageView());
    descriptorSetGenerator.setImage(1, depthImageDesc);

    descriptorSetGenerator.setUniformBuffer(2, *m_uniformBuffer);

    if (!m_rayTracingDescriptorSet)
        m_rayTracingDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_rayTracingDescriptorSetLayout));
    m_rayTracingDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

    {
        Wolf::DescriptorSetGenerator descriptorSetGenerator(m_outputMaskDescriptorSetLayoutGenerator.getDescriptorLayouts());
        Wolf::DescriptorSetGenerator::ImageDescription shadowMaskDesc;
        shadowMaskDesc.imageLayout = Wolf::ImageLayout::GENERAL;
        shadowMaskDesc.imageView = m_outputImage->getDefaultImageView();
        descriptorSetGenerator.setImage(0, shadowMaskDesc);

        m_outputMaskDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
    }
}
