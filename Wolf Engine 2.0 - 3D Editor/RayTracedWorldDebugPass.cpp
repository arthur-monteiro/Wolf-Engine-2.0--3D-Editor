#include "RayTracedWorldDebugPass.h"

#include <CameraInterface.h>
#include <CameraList.h>
#include <DebugMarker.h>
#include <DescriptorSetGenerator.h>
#include <GraphicCameraInterface.h>
#include <MaterialsGPUManager.h>
#include <RayTracingShaderGroupGenerator.h>
#include <RenderMeshList.h>

#include "CommonLayouts.h"
#include "GameContext.h"
#include "Vertex2DTextured.h"

RayTracedWorldDebugPass::RayTracedWorldDebugPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<UpdateRayTracedWorldPass>& updateRayTracedWorldPass,
        const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager)
    : m_editorParams(editorParams), m_preDepthPass(preDepthPass), m_updateRayTracedWorldPass(updateRayTracedWorldPass), m_rayTracedWorldManager(rayTracedWorldManager)
{
}

void RayTracedWorldDebugPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::RAY_TRACING, false /* isTransient */));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

    Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
    m_rayTracedWorldManager->addRayGenShaderCode(shaderCodeToAdd, 2);
    m_rayGenShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedWorldDebug/shader.rgen", {}, 1, -1, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_rayMissShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedWorldDebug/shader.rmiss", {}, 1, -1, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_closestHitShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedWorldDebug/shader.rchit", {}, 1, 3, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));

    m_rayTracingDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::RAYGEN, 0); // output image
    m_rayTracingDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 1); // display options
    m_rayTracingDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    createOutputImage(context);
    m_displayOptionsUniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));
    createPipeline();
    createDescriptorSet();

    DrawRectInterface::initializeResources(context);
}

void RayTracedWorldDebugPass::resize(const Wolf::InitializationContext& context)
{
    createOutputImage(context);
    createDescriptorSet();

    DrawRectInterface::createDescriptorSet(context);
}

void RayTracedWorldDebugPass::record(const Wolf::RecordContext& context)
{
    const GameContext* gameContext = static_cast<const GameContext*>(context.gameContext);
    if (!m_rayTracedWorldManager->hasInstance() ||
        (gameContext->displayType != GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_ALBEDO && gameContext->displayType != GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_INSTANCE_ID
            && gameContext->displayType != GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_PRIMITIVE_ID))
    {
        m_wasEnabledThisFrame = false;
        return;
    }

    UniformBufferData uniformBufferData{};
    uniformBufferData.displayType = static_cast<uint32_t>(gameContext->displayType);
    uniformBufferData.screenOffsetX = m_editorParams->getRenderOffsetLeft();
    uniformBufferData.screenOffsetY = m_editorParams->getRenderOffsetTop();
    m_displayOptionsUniformBuffer->transferCPUMemory(&uniformBufferData, sizeof(uniformBufferData), 0);

    const Wolf::CameraInterface* camera = context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN);

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::rayTracePassDebugColor, "Ray Traced World Debug Pass");

    m_outputImage->transitionImageLayout(*m_commandBuffer, { Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1,
        0, 1, Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL });

    m_commandBuffer->bindPipeline(m_pipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_rayTracingDescriptorSet.createConstNonOwnerResource(), 0, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(camera->getDescriptorSet(), 1, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(m_rayTracedWorldManager->getDescriptorSet(), 2, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 3, *m_pipeline);

    m_commandBuffer->traceRays(m_shaderBindingTable.createConstNonOwnerResource(), { m_editorParams->getRenderWidth(), m_editorParams->getRenderHeight(), 1 });

    m_outputImage->transitionImageLayout(*m_commandBuffer, Wolf::Image::SampledInFragmentShader(0, Wolf::ImageLayout::GENERAL));

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_wasEnabledThisFrame = true;
}

void RayTracedWorldDebugPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_wasEnabledThisFrame)
        return;

    std::vector<const Wolf::Semaphore*> waitSemaphores{ /*m_preDepthPass->getSemaphore()*/ };
    const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
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

void RayTracedWorldDebugPass::createOutputImage(const Wolf::InitializationContext& context)
{
    Wolf::CreateImageInfo createInfo{};
    createInfo.extent = { context.swapChainWidth, context.swapChainHeight, 1 };
    createInfo.usage = Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::STORAGE;
    createInfo.format = Wolf::Format::R8G8B8A8_UNORM;
    createInfo.mipLevelCount = 1;
    m_outputImage.reset(Wolf::Image::createImage(createInfo));
    m_outputImage->setImageLayout(Wolf::Image::SampledInFragmentShader(0));
}

void RayTracedWorldDebugPass::createPipeline()
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
            RayTracedWorldManager::getDescriptorSetLayout().createConstNonOwnerResource(),
            Wolf::MaterialsGPUManager::getDescriptorSetLayout().createConstNonOwnerResource()
        };
    m_pipeline.reset(Wolf::Pipeline::createRayTracingPipeline(pipelineCreateInfo, descriptorSetLayouts));

    m_shaderBindingTable.reset(new Wolf::ShaderBindingTable(static_cast<uint32_t>(shaders.size()), *m_pipeline));
}

void RayTracedWorldDebugPass::createDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts());
    const Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc(Wolf::ImageLayout::GENERAL, m_outputImage->getDefaultImageView());
    descriptorSetGenerator.setImage(0, outputImageDesc);
    descriptorSetGenerator.setUniformBuffer(1, *m_displayOptionsUniformBuffer);

    if (!m_rayTracingDescriptorSet)
        m_rayTracingDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_rayTracingDescriptorSetLayout));
    m_rayTracingDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}