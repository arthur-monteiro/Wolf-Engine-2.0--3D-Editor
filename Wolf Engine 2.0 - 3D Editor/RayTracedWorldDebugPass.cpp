#include "RayTracedWorldDebugPass.h"

#include <CameraInterface.h>
#include <CameraList.h>
#include <DebugMarker.h>
#include <DescriptorSetGenerator.h>
#include <GraphicCameraInterface.h>
#include <RayTracingShaderGroupGenerator.h>
#include <RenderMeshList.h>

#include "CommonLayouts.h"
#include "GameContext.h"
#include "Vertex2DTextured.h"

RayTracedWorldDebugPass::RayTracedWorldDebugPass(const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass)
: m_preDepthPass(preDepthPass)
{
    m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));
}

void RayTracedWorldDebugPass::setTLAS(const Wolf::ResourceNonOwner<Wolf::TopLevelAccelerationStructure>& topLevelAccelerationStructure)
{
    m_topLevelAccelerationStructure = topLevelAccelerationStructure;
    createRayTracingDescriptorSet();
}

void RayTracedWorldDebugPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::RAY_TRACING, false /* isTransient */));

    m_rayGenShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedWorldDebug/shader.rgen", {}, 1));
    m_rayMissShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedWorldDebug/shader.rmiss"));
    m_closestHitShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedWorldDebug/shader.rchit"));

    m_rayTracingDescriptorSetLayoutGenerator.addAccelerationStructure(Wolf::ShaderStageFlagBits::RAYGEN | Wolf::ShaderStageFlagBits::CLOSEST_HIT, 0); // TLAS
    m_rayTracingDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::RAYGEN,                                                   1); // output image
    m_rayTracingDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    createOutputImage(context);
    createRayTracingPipeline();

    m_drawRectDescriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
    m_drawRectDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_drawRectDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    m_drawRectSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR));

    createDrawRectDescriptorSet(context);
}

void RayTracedWorldDebugPass::resize(const Wolf::InitializationContext& context)
{
    createOutputImage(context);
    if (m_topLevelAccelerationStructure)
    {
        createRayTracingDescriptorSet();
    }
}

void RayTracedWorldDebugPass::record(const Wolf::RecordContext& context)
{
    const GameContext* gameContext = static_cast<const GameContext*>(context.gameContext);
    if (!m_topLevelAccelerationStructure || gameContext->displayType != GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_DEPTH)
    {
        m_wasEnabledThisFrame = false;
        return;
    }

    const Wolf::CameraInterface* camera = context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN);

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::rayTracePassDebugColor, "Ray Traced World Debug Pass");

    m_outputImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

    m_commandBuffer->bindPipeline(m_rayTracingPipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_rayTracingDescriptorSet.createConstNonOwnerResource(), 0, *m_rayTracingPipeline);
    m_commandBuffer->bindDescriptorSet(camera->getDescriptorSet(), 1, *m_rayTracingPipeline);

    m_commandBuffer->traceRays(m_shaderBindingTable.createConstNonOwnerResource(), { m_outputImage->getExtent().width, m_outputImage->getExtent().height, 1 });

    m_outputImage->transitionImageLayout(*m_commandBuffer, Wolf::Image::SampledInFragmentShader(0, VK_IMAGE_LAYOUT_GENERAL));

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_wasEnabledThisFrame = true;
}

void RayTracedWorldDebugPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_wasEnabledThisFrame)
        return;

    const std::vector<const Wolf::Semaphore*> waitSemaphores{ /*m_preDepthPass->getSemaphore()*/ };
    const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);

    bool anyShaderModified = m_rayGenShaderParser->compileIfFileHasBeenModified();
    if (m_rayMissShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;
    if (m_closestHitShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;

    if (anyShaderModified)
    {
        context.graphicAPIManager->waitIdle();
        createRayTracingPipeline();
    }
}

void RayTracedWorldDebugPass::bindDescriptorSetToDrawOutput(const Wolf::CommandBuffer& commandBuffer, const Wolf::Pipeline& pipeline)
{
    commandBuffer.bindDescriptorSet(m_drawRectDescriptorSet.createConstNonOwnerResource(), 0, pipeline);
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

void RayTracedWorldDebugPass::createRayTracingPipeline()
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
        { m_rayTracingDescriptorSetLayout.createConstNonOwnerResource(), Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource() };
    m_rayTracingPipeline.reset(Wolf::Pipeline::createRayTracingPipeline(pipelineCreateInfo, descriptorSetLayouts));

    m_shaderBindingTable.reset(new Wolf::ShaderBindingTable(static_cast<uint32_t>(shaders.size()), *m_rayTracingPipeline));
}

void RayTracedWorldDebugPass::createRayTracingDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setAccelerationStructure(0, *m_topLevelAccelerationStructure);
    const Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc(VK_IMAGE_LAYOUT_GENERAL, m_outputImage->getDefaultImageView());
    descriptorSetGenerator.setImage(1, outputImageDesc);

    if (!m_rayTracingDescriptorSet)
        m_rayTracingDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_rayTracingDescriptorSetLayout));
    m_rayTracingDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void RayTracedWorldDebugPass::createDrawRectDescriptorSet(const Wolf::InitializationContext& context)
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_drawRectDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_outputImage->getDefaultImageView(), *m_drawRectSampler);

    if (!m_drawRectDescriptorSet)
        m_drawRectDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_drawRectDescriptorSetLayout));
    m_drawRectDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}
