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
        const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager, const Wolf::ResourceNonOwner<GPUNoiseManager>& noiseManager)
    : m_editorParams(editorParams), m_preDepthPass(preDepthPass), m_updateRayTracedWorldPass(updateRayTracedWorldPass), m_rayTracedWorldManager(rayTracedWorldManager), m_noiseManager(noiseManager)
{
}

void RayTracedShadowsPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

    /* Ray tracing pass */
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
    m_rayTracingDescriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::RAYGEN, 3); // vogel disk noise
    m_rayTracingDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::RAYGEN, 4); // occluder distance
    m_rayTracingDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    /* Penumbra scale compute */
    m_penumbraScaleComputeShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedShadows/computePenumbraScale.comp", {}, 1));

    m_penumbraScaleComputeDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::COMPUTE, 0); // output image
    m_penumbraScaleComputeDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::COMPUTE, 1); // shadow mask
    m_penumbraScaleComputeDescriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE, 2, 1); // depth image
    m_penumbraScaleComputeDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::COMPUTE, 3); // occluder distance
    m_penumbraScaleComputeDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 4); // uniform buffer
    m_penumbraScaleComputeDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_penumbraScaleComputeDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    /* Blur */
    m_denoiseHorizontalBlurShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedShadows/denoiseBlur.comp", { "HORIZONTAL" }, 1));
    m_denoiseVerticalBlurShaderParser.reset(new Wolf::ShaderParser("Shaders/rayTracedShadows/denoiseBlur.comp", { "VERTICAL" }, 1));

    m_denoiseBlurDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::COMPUTE, 0); // output image
    m_denoiseBlurDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::COMPUTE, 1); // input shadow mask
    m_denoiseBlurDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::COMPUTE, 2); // input penumbra scale
    m_denoiseBlurDescriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE, 3, 1); // depth image
    m_denoiseBlurDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_denoiseBlurDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    /* Output reading */
    m_outputMaskDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
    m_outputMaskDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_outputMaskDescriptorSetLayoutGenerator.getDescriptorLayouts()));
    m_outputMaskDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_outputMaskDescriptorSetLayout));

    /* Resources */
    createOutputImages(context);
    m_rayTracingUniformBuffer.reset(new Wolf::UniformBuffer(sizeof(RayTracingUniformBufferData)));
    m_penumbraScaleComputeUniformBuffer.reset(new Wolf::UniformBuffer(sizeof(PenumbraScaleComputeUniformBuffer)));
    createPipelines();
    createDescriptorSets();
}

void RayTracedShadowsPass::resize(const Wolf::InitializationContext& context)
{
    createOutputImages(context);
    createDescriptorSets();
}

void RayTracedShadowsPass::record(const Wolf::RecordContext& context)
{
    const GameContext* gameContext = static_cast<const GameContext*>(context.m_gameContext);
    if (!m_rayTracedWorldManager->hasInstance() || gameContext->displayType != GameContext::DisplayType::LIGHTING)
    {
        m_wasEnabledThisFrame = false;
        return;
    }

    uint32_t sunLightCount = context.m_lightManager->getSunLightCount();
    if (sunLightCount == 0)
    {
        return;
    }

    glm::vec3 sunDirection = context.m_lightManager->getSunLightInfo(0).direction;
    float sunAngle = context.m_lightManager->getSunLightInfo(0).angle;

    RayTracingUniformBufferData rayTracingUniformBufferData{};

    rayTracingUniformBufferData.sunDirectionAndNoiseIndex = glm::vec4(sunDirection, static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 3.14f * 2.0f);
    rayTracingUniformBufferData.sunAreaAngle = sunAngle;
    rayTracingUniformBufferData.screenOffsetX = m_editorParams->getRenderOffsetLeft();
    rayTracingUniformBufferData.screenOffsetY = m_editorParams->getRenderOffsetTop();
    m_rayTracingUniformBuffer->transferCPUMemory(&rayTracingUniformBufferData, sizeof(rayTracingUniformBufferData), 0);

    PenumbraScaleComputeUniformBuffer penumbraScaleComputeUniformBuffer{};
    penumbraScaleComputeUniformBuffer.m_sunAreaAngle = sunAngle;
    penumbraScaleComputeUniformBuffer.m_screenHeight = m_editorParams->getRenderHeight();
    const float fovY = context.m_cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN)->getFOV();
    penumbraScaleComputeUniformBuffer.m_tanHalfFov = glm::tan(fovY * 0.5);
    m_penumbraScaleComputeUniformBuffer->transferCPUMemory(&penumbraScaleComputeUniformBuffer, sizeof(penumbraScaleComputeUniformBuffer), 0);

    const Wolf::CameraInterface* camera = context.m_cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN);

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::rayTracePassDebugColor, "Ray Traced Shadows - Tracing");

    m_commandBuffer->bindPipeline(m_rayTracingPipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_rayTracingDescriptorSet.createConstNonOwnerResource(), 0, *m_rayTracingPipeline);
    m_commandBuffer->bindDescriptorSet(camera->getDescriptorSet(), 1, *m_rayTracingPipeline);
    m_commandBuffer->bindDescriptorSet(m_rayTracedWorldManager->getDescriptorSet(), 2, *m_rayTracingPipeline);

    m_commandBuffer->traceRays(m_shaderBindingTable.createConstNonOwnerResource(), { m_editorParams->getRenderWidth(), m_editorParams->getRenderHeight(), 1 });

    m_shadowMaskImage0->transitionImageLayout(*m_commandBuffer,
                { Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
                    Wolf::ImageLayout::GENERAL });

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::computePassDebugColor, "Ray Traced Shadows - Compute penumbra scale");

    constexpr Wolf::Extent3D dispatchGroups = { 16, 16, 1 };
    const uint32_t groupSizeX = m_shadowMaskImage1->getExtent().width % dispatchGroups.width != 0 ? m_shadowMaskImage1->getExtent().width / dispatchGroups.width + 1 : m_shadowMaskImage1->getExtent().width / dispatchGroups.width;
    const uint32_t groupSizeY = m_shadowMaskImage1->getExtent().height % dispatchGroups.height != 0 ? m_shadowMaskImage1->getExtent().height / dispatchGroups.height + 1 : m_shadowMaskImage1->getExtent().height / dispatchGroups.height;

    m_commandBuffer->bindPipeline(m_penumbraScaleComputePipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_penumbraScaleComputeDescriptorSet.createConstNonOwnerResource(), 0, *m_penumbraScaleComputePipeline);
    m_commandBuffer->bindDescriptorSet(context.m_cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN)->getDescriptorSet(), 1, *m_penumbraScaleComputePipeline);

    m_commandBuffer->dispatch(groupSizeX, groupSizeY, dispatchGroups.depth);

    m_penumbraScaleImage->transitionImageLayout(*m_commandBuffer,
                { Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1, Wolf::ImageLayout::GENERAL });

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::computePassDebugColor, "Ray Traced Shadows - Blur");

    for (uint32_t passIdx = 0; passIdx < 10; ++passIdx)
    {
        if (passIdx % 2 == 0)
        {
            m_commandBuffer->bindPipeline(m_denoiseHorizontalBlurPipeline.createConstNonOwnerResource());
            m_commandBuffer->bindDescriptorSet(m_denoiseFirstDescriptorSet.createConstNonOwnerResource(), 0, *m_denoiseHorizontalBlurPipeline);
            m_commandBuffer->bindDescriptorSet(context.m_cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN)->getDescriptorSet(), 1, *m_denoiseHorizontalBlurPipeline);
        }
        else
        {
            m_commandBuffer->bindPipeline(m_denoiseVerticalBlurPipeline.createConstNonOwnerResource());
            m_commandBuffer->bindDescriptorSet(m_denoiseSecondDescriptorSet.createConstNonOwnerResource(), 0, *m_denoiseVerticalBlurPipeline);
            m_commandBuffer->bindDescriptorSet(context.m_cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN)->getDescriptorSet(), 1, *m_denoiseVerticalBlurPipeline);
        }

        m_commandBuffer->dispatch(groupSizeX, groupSizeY, dispatchGroups.depth);

        if (passIdx % 2 == 0)
        {
            m_shadowMaskImage1->transitionImageLayout(*m_commandBuffer,
                { Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
                    Wolf::ImageLayout::GENERAL });

            m_shadowMaskImage0->transitionImageLayout(*m_commandBuffer,
                { Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
                    Wolf::ImageLayout::GENERAL });
        }
        else
        {
            VkPipelineStageFlags outputStage = passIdx == 3 ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            m_shadowMaskImage0->transitionImageLayout(*m_commandBuffer,
                { Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_READ_BIT, outputStage, 0, 1, 0, 1,
                    Wolf::ImageLayout::GENERAL });

            m_shadowMaskImage1->transitionImageLayout(*m_commandBuffer,
                { Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
                    Wolf::ImageLayout::GENERAL });
        }
    }

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
    if (m_penumbraScaleComputeShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;
    if (m_denoiseHorizontalBlurShaderParser->compileIfFileHasBeenModified(m_denoiseHorizontalBlurShaderParser->getCurrentConditionsBlocks()))
        anyShaderModified = true;
    if (m_denoiseVerticalBlurShaderParser->compileIfFileHasBeenModified(m_denoiseVerticalBlurShaderParser->getCurrentConditionsBlocks()))
        anyShaderModified = true;

    if (anyShaderModified)
    {
        context.graphicAPIManager->waitIdle();
        createPipelines();
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

void RayTracedShadowsPass::createOutputImages(const Wolf::InitializationContext& context)
{
    Wolf::CreateImageInfo shadowMaskCreateInfo{};
    shadowMaskCreateInfo.extent = { context.swapChainWidth, context.swapChainHeight, 1 };
    shadowMaskCreateInfo.usage = Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::STORAGE;
    shadowMaskCreateInfo.format = Wolf::Format::R16_SFLOAT;
    shadowMaskCreateInfo.mipLevelCount = 1;
    m_shadowMaskImage1.reset(Wolf::Image::createImage(shadowMaskCreateInfo));
    m_shadowMaskImage1->setImageLayout({ Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
        Wolf::ImageLayout::UNDEFINED });

    m_shadowMaskImage0.reset(Wolf::Image::createImage(shadowMaskCreateInfo));
    m_shadowMaskImage0->setImageLayout({ Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
        Wolf::ImageLayout::UNDEFINED });

    Wolf::CreateImageInfo occluderDistanceCreateInfo{};
    occluderDistanceCreateInfo.extent = { context.swapChainWidth, context.swapChainHeight, 1 };
    occluderDistanceCreateInfo.usage = Wolf::ImageUsageFlagBits::STORAGE;
    occluderDistanceCreateInfo.format = Wolf::Format::R16_SFLOAT;
    occluderDistanceCreateInfo.mipLevelCount = 1;
    m_occluderDistanceImage.reset(Wolf::Image::createImage(occluderDistanceCreateInfo));
    m_occluderDistanceImage->setImageLayout({ Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
        Wolf::ImageLayout::UNDEFINED });

    m_shadowMaskImage0.reset(Wolf::Image::createImage(shadowMaskCreateInfo));
    m_shadowMaskImage0->setImageLayout({ Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
        Wolf::ImageLayout::UNDEFINED });

    Wolf::CreateImageInfo penumbraScaleCreateInfo{};
    penumbraScaleCreateInfo.extent = { context.swapChainWidth, context.swapChainHeight, 1 };
    penumbraScaleCreateInfo.usage = Wolf::ImageUsageFlagBits::STORAGE;
    penumbraScaleCreateInfo.format = Wolf::Format::R8_UNORM;
    penumbraScaleCreateInfo.mipLevelCount = 1;
    m_penumbraScaleImage.reset(Wolf::Image::createImage(penumbraScaleCreateInfo));
    m_penumbraScaleImage->setImageLayout({ Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0, 1,
        Wolf::ImageLayout::UNDEFINED });
}

void RayTracedShadowsPass::createPipelines()
{
    // Ray tracing
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
        m_rayTracingPipeline.reset(Wolf::Pipeline::createRayTracingPipeline(pipelineCreateInfo, descriptorSetLayouts));

        m_shaderBindingTable.reset(new Wolf::ShaderBindingTable(static_cast<uint32_t>(shaders.size()), *m_rayTracingPipeline));
    }

    /* Denoising */

    // Penumbra scale
    {
        Wolf::ShaderCreateInfo shaderCreateInfo{};
        std::vector<char> shaderCode;
        m_penumbraScaleComputeShaderParser->readCompiledShader(shaderCode);
        shaderCreateInfo.shaderCode = shaderCode;
        shaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;

        std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts;
        descriptorSetLayouts.reserve(2);
        descriptorSetLayouts.emplace_back(m_penumbraScaleComputeDescriptorSetLayout.createConstNonOwnerResource());
        descriptorSetLayouts.emplace_back(Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource());

        m_penumbraScaleComputePipeline.reset(Wolf::Pipeline::createComputePipeline(shaderCreateInfo, descriptorSetLayouts));
    }

    // Horizontal blur
    {
        Wolf::ShaderCreateInfo denoiseShaderCreateInfo{};
        std::vector<char> denoiseShaderCode;
        m_denoiseHorizontalBlurShaderParser->readCompiledShader(denoiseShaderCode);
        denoiseShaderCreateInfo.shaderCode = denoiseShaderCode;
        denoiseShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;

        std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> denoiseDescriptorSetLayouts;
        denoiseDescriptorSetLayouts.reserve(2);
        denoiseDescriptorSetLayouts.emplace_back(m_denoiseBlurDescriptorSetLayout.createConstNonOwnerResource());
        denoiseDescriptorSetLayouts.emplace_back(Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource());

        m_denoiseHorizontalBlurPipeline.reset(Wolf::Pipeline::createComputePipeline(denoiseShaderCreateInfo, denoiseDescriptorSetLayouts));
    }
    // Vertical blur
    {
        Wolf::ShaderCreateInfo denoiseShaderCreateInfo{};
        std::vector<char> denoiseShaderCode;
        m_denoiseVerticalBlurShaderParser->readCompiledShader(denoiseShaderCode);
        denoiseShaderCreateInfo.shaderCode = denoiseShaderCode;
        denoiseShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;

        std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> denoiseDescriptorSetLayouts;
        denoiseDescriptorSetLayouts.reserve(2);
        denoiseDescriptorSetLayouts.emplace_back(m_denoiseBlurDescriptorSetLayout.createConstNonOwnerResource());
        denoiseDescriptorSetLayouts.emplace_back(Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource());

        m_denoiseVerticalBlurPipeline.reset(Wolf::Pipeline::createComputePipeline(denoiseShaderCreateInfo, denoiseDescriptorSetLayouts));
    }
}

void RayTracedShadowsPass::createDescriptorSets()
{
    {
        Wolf::DescriptorSetGenerator descriptorSetGenerator(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts());
        const Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc(Wolf::ImageLayout::GENERAL, m_shadowMaskImage0->getDefaultImageView());
        descriptorSetGenerator.setImage(0, outputImageDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription depthImageDesc(Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL, m_preDepthPass->getOutput()->getDefaultImageView());
        descriptorSetGenerator.setImage(1, depthImageDesc);

        descriptorSetGenerator.setUniformBuffer(2, *m_rayTracingUniformBuffer);
        descriptorSetGenerator.setCombinedImageSampler(3, Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL, m_noiseManager->getVogelDiskImage()->getDefaultImageView(), *m_noiseManager->getVogelDiskSampler());

        const Wolf::DescriptorSetGenerator::ImageDescription occluderDistanceImageDesc(Wolf::ImageLayout::GENERAL, m_occluderDistanceImage->getDefaultImageView());
        descriptorSetGenerator.setImage(4, occluderDistanceImageDesc);

        if (!m_rayTracingDescriptorSet)
            m_rayTracingDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_rayTracingDescriptorSetLayout));
        m_rayTracingDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
    }

    {
        Wolf::DescriptorSetGenerator descriptorSetGenerator(m_penumbraScaleComputeDescriptorSetLayoutGenerator.getDescriptorLayouts());

        const Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc(Wolf::ImageLayout::GENERAL, m_penumbraScaleImage->getDefaultImageView());
        descriptorSetGenerator.setImage(0, outputImageDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription inputImageDesc(Wolf::ImageLayout::GENERAL, m_shadowMaskImage0->getDefaultImageView());
        descriptorSetGenerator.setImage(1, inputImageDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription depthImageDesc(Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL, m_preDepthPass->getOutput()->getDefaultImageView());
        descriptorSetGenerator.setImage(2, depthImageDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription occluderDistanceImageDesc(Wolf::ImageLayout::GENERAL, m_occluderDistanceImage->getDefaultImageView());
        descriptorSetGenerator.setImage(3, occluderDistanceImageDesc);

        descriptorSetGenerator.setUniformBuffer(4, *m_penumbraScaleComputeUniformBuffer);

        if (!m_penumbraScaleComputeDescriptorSet)
            m_penumbraScaleComputeDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_penumbraScaleComputeDescriptorSetLayout));
        m_penumbraScaleComputeDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
    }

    {
        Wolf::DescriptorSetGenerator descriptorSetGenerator(m_denoiseBlurDescriptorSetLayoutGenerator.getDescriptorLayouts());

        const Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc(Wolf::ImageLayout::GENERAL, m_shadowMaskImage1->getDefaultImageView());
        descriptorSetGenerator.setImage(0, outputImageDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription inputImageDesc(Wolf::ImageLayout::GENERAL, m_shadowMaskImage0->getDefaultImageView());
        descriptorSetGenerator.setImage(1, inputImageDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription inputPenumbraScaleDesc(Wolf::ImageLayout::GENERAL, m_penumbraScaleImage->getDefaultImageView());
        descriptorSetGenerator.setImage(2, inputPenumbraScaleDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription depthImageDesc(Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL, m_preDepthPass->getOutput()->getDefaultImageView());
        descriptorSetGenerator.setImage(3, depthImageDesc);

        if (!m_denoiseFirstDescriptorSet)
            m_denoiseFirstDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_denoiseBlurDescriptorSetLayout));
        m_denoiseFirstDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
    }

    {
        Wolf::DescriptorSetGenerator descriptorSetGenerator(m_denoiseBlurDescriptorSetLayoutGenerator.getDescriptorLayouts());

        const Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc(Wolf::ImageLayout::GENERAL, m_shadowMaskImage0->getDefaultImageView());
        descriptorSetGenerator.setImage(0, outputImageDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription inputImageDesc(Wolf::ImageLayout::GENERAL, m_shadowMaskImage1->getDefaultImageView());
        descriptorSetGenerator.setImage(1, inputImageDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription inputPenumbraScaleDesc(Wolf::ImageLayout::GENERAL, m_penumbraScaleImage->getDefaultImageView());
        descriptorSetGenerator.setImage(2, inputPenumbraScaleDesc);

        const Wolf::DescriptorSetGenerator::ImageDescription depthImageDesc(Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL, m_preDepthPass->getOutput()->getDefaultImageView());
        descriptorSetGenerator.setImage(3, depthImageDesc);

        if (!m_denoiseSecondDescriptorSet)
            m_denoiseSecondDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_denoiseBlurDescriptorSetLayout));
        m_denoiseSecondDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
    }

    {
        Wolf::DescriptorSetGenerator descriptorSetGenerator(m_outputMaskDescriptorSetLayoutGenerator.getDescriptorLayouts());
        Wolf::DescriptorSetGenerator::ImageDescription shadowMaskDesc;
        shadowMaskDesc.imageLayout = Wolf::ImageLayout::GENERAL;
        shadowMaskDesc.imageView = m_shadowMaskImage1->getDefaultImageView();
        descriptorSetGenerator.setImage(0, shadowMaskDesc);

        m_outputMaskDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
    }
}
