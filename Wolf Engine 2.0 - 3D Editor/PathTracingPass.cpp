#include "PathTracingPass.h"

#include <random>
#include <xxh64.hpp>

#include <CameraList.h>
#include <ConfigurationHelper.h>
#include <DebugMarker.h>
#include <fstream>
#include <GraphicCameraInterface.h>
#include <ImageFileLoader.h>
#include <LightManager.h>
#include <MaterialsGPUManager.h>
#include <RayTracingShaderGroupGenerator.h>
#include <ShaderBindingTable.h>
#include <ShaderParser.h>
#include <stb_image_write.h>
#include <glm/gtc/constants.hpp>

#include "CommonLayouts.h"
#include "GameContext.h"

PathTracingPass::PathTracingPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<ComputeSkyCubeMapPass>& computeSkyCubeMapPass,
    const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager)
: m_editorParams(editorParams), m_preDepthPass(preDepthPass), m_computeSkyCubeMapPass(computeSkyCubeMapPass), m_rayTracedWorldManager(rayTracedWorldManager)
{
}

void PathTracingPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::RAY_TRACING, false /* isTransient */));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

    Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
    m_rayTracedWorldManager->addRayGenShaderCode(shaderCodeToAdd, 2);
    m_rayGenShaderParser.reset(new Wolf::ShaderParser("Shaders/pathTracing/shader.rgen", {}, 1, 3, 4,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_rayMissShaderParser.reset(new Wolf::ShaderParser("Shaders/pathTracing/shader.rmiss", {}, 1, 3, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_closestHitShaderParser.reset(new Wolf::ShaderParser("Shaders/pathTracing/shader.rchit", {}, 1, 3, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));

    m_descriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::RAYGEN, 0); // output image
    m_descriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::RAYGEN, 1); // input image
    m_descriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::RAYGEN, 2); // noise map
    m_descriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 3); // info
    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

    createImages(context);
    createNoiseImage();
    createPipeline();

    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));

    createDescriptorSet();

    DrawRectInterface::initializeResources(context);
}

void PathTracingPass::resize(const Wolf::InitializationContext& context)
{
    createImages(context);
    createDescriptorSet();

    DrawRectInterface::createDescriptorSet(context);
}

void PathTracingPass::record(const Wolf::RecordContext& context)
{
    const GameContext* gameContext = static_cast<const GameContext*>(context.gameContext);
    if (!m_rayTracedWorldManager->hasInstance() || gameContext->displayType != GameContext::DisplayType::PATH_TRACING)
    {
        m_wasEnabledThisFrame = false;
        return;
    }
    const Wolf::CameraInterface* camera = context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN);
    uint64_t currentInfoHash = computeInfoHash(camera->getViewMatrix());

    if (m_isFirstRecord)
    {
        initImagesContent(currentInfoHash, gameContext->currentSceneName);
    }

    m_isFirstRecord = false;

    if (m_previousInfoHash != currentInfoHash)
    {
        if (m_previousInfoHash != 0ul)
        {
            if (!m_hasInfoChangedFromInit)
            {
                // Save current state
                std::filesystem::create_directories("PathTracingResults/"  + gameContext->currentSceneName);

                std::vector<uint8_t> fullImageData;
                m_outputImage->exportToBuffer(fullImageData);
                float* fullImageDataAsFloats = reinterpret_cast<float*>(fullImageData.data());

                uint32_t channelCount = 4;

                const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();
                std::vector<float> renderImageData(renderViewport.width * renderViewport.height * channelCount);

                for (uint32_t y = 0; y < static_cast<uint32_t>(renderViewport.height); y++)
                {
                    uint32_t offsetInFullImageData = ((y + static_cast<uint32_t>(renderViewport.y)) * m_outputImage->getExtent().width * channelCount) + static_cast<uint32_t>(renderViewport.x) * channelCount;
                    float* src = &fullImageDataAsFloats[offsetInFullImageData];
                    float* dst = &renderImageData[y * renderViewport.width * channelCount];
                    uint32_t lineCopySize = renderViewport.width * channelCount * sizeof(float);

                    memcpy(dst, src, lineCopySize);
                }

                std::string outFilePath = "PathTracingResults/"  + gameContext->currentSceneName + "/screenshot.hdr";
                stbi_write_hdr(outFilePath.c_str(), renderViewport.width, renderViewport.height, channelCount, renderImageData.data());

                std::string infoFilePath = "PathTracingResults/"  + gameContext->currentSceneName + "/info.txt";
                Wolf::ConfigurationHelper::writeInfoToFile(infoFilePath, "FrameCount", m_internalFrameIdx);
                Wolf::ConfigurationHelper::writeInfoToFile(infoFilePath, "DitherIdx", m_ditherIdx);
                Wolf::ConfigurationHelper::writeInfoToFile(infoFilePath, "InfoHash", m_previousInfoHash);
            }
            m_hasInfoChangedFromInit = true;

            m_internalFrameIdx = 0;
        }

        m_previousInfoHash = currentInfoHash;
    }

    UniformBufferData uniformBufferData{};
    uniformBufferData.noiseIdx = m_internalFrameIdx % NOISE_VECTOR_COUNT_PER_BOUNCE;
    uniformBufferData.ditherX = m_ditherIdx % DITHER_SIZE;
    uniformBufferData.ditherY = m_ditherIdx / DITHER_SIZE;
    uniformBufferData.frameIdx = m_internalFrameIdx;
    uniformBufferData.screenOffsetX = m_editorParams->getRenderOffsetLeft();
    uniformBufferData.screenOffsetY = m_editorParams->getRenderOffsetTop();
    m_uniformBuffer->transferCPUMemory(&uniformBufferData, sizeof(uniformBufferData), 0);

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::rayTracePassDebugColor, "Path tracing Pass");

    m_outputImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1,
        0, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

    m_commandBuffer->bindPipeline(m_pipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_descriptorSet.createConstNonOwnerResource(), 0, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(camera->getDescriptorSet(), 1, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(m_rayTracedWorldManager->getDescriptorSet(), 2, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 3, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(context.lightManager->getDescriptorSet().createConstNonOwnerResource(), 4, *m_pipeline);

    m_commandBuffer->traceRays(m_shaderBindingTable.createConstNonOwnerResource(), { m_editorParams->getRenderWidth(), m_editorParams->getRenderHeight(), 1 });

    VkImageCopy copyRegion{};
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

    Wolf::Extent3D extent = m_inputImage->getExtent();
    copyRegion.extent = { extent.width, extent.height, extent.depth };

    m_inputImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0, 1, 0, 1, VK_IMAGE_LAYOUT_GENERAL });
    m_inputImage->recordCopyGPUImage(*m_outputImage, copyRegion, *m_commandBuffer);
    m_inputImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0, 1, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL });

    m_outputImage->transitionImageLayout(*m_commandBuffer, Wolf::Image::SampledInFragmentShader(0, VK_IMAGE_LAYOUT_GENERAL));

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_wasEnabledThisFrame = true;

    m_ditherIdx++;
    if (m_ditherIdx == DITHER_SIZE * DITHER_SIZE)
    {
        if (m_internalFrameIdx % NOISE_VECTOR_COUNT_PER_BOUNCE == NOISE_VECTOR_COUNT_PER_BOUNCE - 1)
        {
            createNoiseImage();
        }

        m_internalFrameIdx++;
        m_ditherIdx = 0;
    }
}

void PathTracingPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_wasEnabledThisFrame)
        return;

    std::vector<const Wolf::Semaphore*> waitSemaphores{ /*m_preDepthPass->getSemaphore()*/ };
    if (m_computeSkyCubeMapPass->wasEnabledThisFrame())
        waitSemaphores.push_back({ m_computeSkyCubeMapPass->getSemaphore(context.swapChainImageIndex) });

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

void PathTracingPass::createImages(const Wolf::InitializationContext& context)
{
    Wolf::CreateImageInfo createInfo{};
    createInfo.extent = { context.swapChainWidth, context.swapChainHeight, 1 };
    createInfo.usage = Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::STORAGE | Wolf::ImageUsageFlagBits::TRANSFER_SRC | Wolf::ImageUsageFlagBits::TRANSFER_DST;
    createInfo.format = Wolf::Format::R32G32B32A32_SFLOAT;
    createInfo.mipLevelCount = 1;
    m_outputImage.reset(Wolf::Image::createImage(createInfo));
    m_outputImage->setImageLayout(Wolf::Image::SampledInFragmentShader(0));

    createInfo.usage = Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::STORAGE | Wolf::ImageUsageFlagBits::TRANSFER_DST;
    m_inputImage.reset(Wolf::Image::createImage(createInfo));
    m_inputImage->setImageLayout(Wolf::Image::SampledInFragmentShader(0));

    Wolf::CommandBuffer* commandBuffer = Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, true);
    commandBuffer->beginCommandBuffer();
}

float jitter()
{
    static std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());
    static std::uniform_real_distribution distrib(0.0f, 1.0f);
    return distrib(generator);
}

glm::vec3 uniformSampleHemisphere(float r1, float r2)
{
    // cos(theta) = r1 = y
    // cos^2(theta) + sin^2(theta) = 1 -> sin(theta) = sqrtf(1 - cos^2(theta))
    float sinTheta = sqrtf(1 - r1 * r1);
    float phi = 2 * glm::pi<float>() * r2;
    float x = sinTheta * cosf(phi);
    float z = sinTheta * sinf(phi);
    return {x, r1, z};
}

void PathTracingPass::createNoiseImage()
{
    Wolf::CreateImageInfo noiseImageCreateInfo;
    noiseImageCreateInfo.extent = { NOISE_TEXTURE_SIZE_PER_SIDE, NOISE_TEXTURE_SIZE_PER_SIDE, NOISE_TEXTURE_VECTOR_COUNT };
    noiseImageCreateInfo.format = Wolf::Format::R32G32B32A32_SFLOAT;
    noiseImageCreateInfo.mipLevelCount = 1;
    noiseImageCreateInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
    if (!m_noiseImage)
    {
        m_noiseImage.reset(Wolf::Image::createImage(noiseImageCreateInfo));
    }

    std::vector<float> noiseData(NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_VECTOR_COUNT * 4);
    for (int texY = 0; texY < NOISE_TEXTURE_SIZE_PER_SIDE; ++texY)
    {
        for (int texX = 0; texX < NOISE_TEXTURE_SIZE_PER_SIDE; ++texX)
        {
            for (int i = 0; i < NOISE_TEXTURE_VECTOR_COUNT; ++i)
            {
                float r1 = jitter();
                float r2 = jitter();

                glm::vec3 offsetDirection = uniformSampleHemisphere(r1, r2);
                offsetDirection = glm::normalize(offsetDirection);

                const uint32_t idx = texX + texY * NOISE_TEXTURE_SIZE_PER_SIDE + i * (NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_SIZE_PER_SIDE);

                noiseData[4 * idx    ] = offsetDirection.x;
                noiseData[4 * idx + 1] = offsetDirection.y;
                noiseData[4 * idx + 2] = offsetDirection.z;
            }
        }
    }
    m_noiseImage->copyCPUBuffer(reinterpret_cast<unsigned char*>(noiseData.data()), Wolf::Image::SampledInFragmentShader());

    if (!m_noiseSampler)
    {
        m_noiseSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1.0f, VK_FILTER_NEAREST));
    }
}

void PathTracingPass::createPipeline()
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
        m_descriptorSetLayout.createConstNonOwnerResource(),
        Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource(),
        RayTracedWorldManager::getDescriptorSetLayout().createConstNonOwnerResource(),
        Wolf::MaterialsGPUManager::getDescriptorSetLayout().createConstNonOwnerResource(),
        Wolf::LightManager::getDescriptorSetLayout().createConstNonOwnerResource()
    };
    m_pipeline.reset(Wolf::Pipeline::createRayTracingPipeline(pipelineCreateInfo, descriptorSetLayouts));

    m_shaderBindingTable.reset(new Wolf::ShaderBindingTable(static_cast<uint32_t>(shaders.size()), *m_pipeline));
}

void PathTracingPass::createDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
    const Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc(VK_IMAGE_LAYOUT_GENERAL, m_outputImage->getDefaultImageView());
    descriptorSetGenerator.setImage(0, outputImageDesc);
    const Wolf::DescriptorSetGenerator::ImageDescription inputImageDesc(VK_IMAGE_LAYOUT_GENERAL, m_inputImage->getDefaultImageView());
    descriptorSetGenerator.setImage(1, inputImageDesc);
    descriptorSetGenerator.setCombinedImageSampler(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_noiseImage->getDefaultImageView(), *m_noiseSampler);
    descriptorSetGenerator.setUniformBuffer(3, *m_uniformBuffer);

    if (!m_descriptorSet)
        m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
    m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

uint64_t PathTracingPass::computeInfoHash(const glm::mat4& viewMatrix) const
{
    struct Info
    {
        glm::mat4 viewMatrix;
        uint32_t width, height;
    };

    Info info { viewMatrix, m_editorParams->getRenderWidth(), m_editorParams->getRenderHeight() };
    return xxh64::hash(reinterpret_cast<const char*>(&info), sizeof(info), 0);
}

void PathTracingPass::initImagesContent(uint64_t infoHash, const std::string& currentSceneName)
{
    std::string infoFilePath = "PathTracingResults/"  + currentSceneName + "/info.txt";
    {
        struct stat buffer;
        if (stat(infoFilePath.c_str(), &buffer) != 0)
            return; // file doesn't exist
    }


    std::string hashString = Wolf::ConfigurationHelper::readInfoFromFile(infoFilePath, "InfoHash");
    if (hashString != std::to_string(infoHash))
        return; // hash is not the same

    std::string screenShotFilePath = "PathTracingResults/"  + currentSceneName + "/screenshot.hdr";
    {
        struct stat buffer;
        if (stat(screenShotFilePath.c_str(), &buffer) != 0)
            return; // file doesn't exist
    }

    Wolf::ImageFileLoader imageFileLoader(screenShotFilePath, true);

    std::vector<glm::vec4> fullImageData(m_inputImage->getExtent().width * m_inputImage->getExtent().height, glm::vec4(0.0f));
    for (uint32_t y = m_editorParams->getRenderOffsetTop(); y < m_editorParams->getRenderOffsetTop() + m_editorParams->getRenderHeight(); ++y)
    {
        uint32_t fullPixelCopyOffset = y * m_inputImage->getExtent().width + m_editorParams->getRenderOffsetLeft();
        uint32_t fullPixelCopySize = m_editorParams->getRenderWidth();

        uint32_t sourcePixelCopyOffset = (y - m_editorParams->getRenderOffsetTop()) * m_editorParams->getRenderWidth();

        memcpy(&fullImageData[fullPixelCopyOffset], &imageFileLoader.getPixels()[sourcePixelCopyOffset * sizeof(glm::vec4)], fullPixelCopySize * sizeof(glm::vec4));
    }

    m_inputImage->copyCPUBuffer(reinterpret_cast<const unsigned char*>(fullImageData.data()), { VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0, 1, 0 });
    m_outputImage->copyCPUBuffer(reinterpret_cast<const unsigned char*>(fullImageData.data()), Wolf::Image::SampledInFragmentShader(0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

    m_ditherIdx = std::stoi(Wolf::ConfigurationHelper::readInfoFromFile(infoFilePath, "DitherIdx"));
    m_internalFrameIdx = std::stoi(Wolf::ConfigurationHelper::readInfoFromFile(infoFilePath, "FrameCount"));
}
