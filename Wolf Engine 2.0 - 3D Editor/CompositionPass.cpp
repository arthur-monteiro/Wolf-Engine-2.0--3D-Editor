#include "CompositionPass.h"

#include <glm/gtc/packing.hpp>
#include <stb_image_write.h>

#include <DebugMarker.h>
#include <DescriptorSetGenerator.h>
#include <ProfilerCommon.h>

#include "GameContext.h"
#include "Vertex2DTextured.h"

CompositionPass::CompositionPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<ForwardPass>& forwardPass) : m_editorParams(editorParams), m_forwardPass(forwardPass)
{
}

void CompositionPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::COMPUTE, false /* isTransient */));
    createSemaphores(context, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, true);

    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::STORAGE_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE, 0, 1); // forward result
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::STORAGE_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE, 1, 1); // UI result
    m_descriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::COMPUTE, 2); // LUT
    m_descriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 3); // uniform buffer
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::STORAGE_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE, 4, 1); // output image
    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

    m_descriptorSets.resize(context.swapChainImageCount);
    for (uint32_t i = 0; i < m_descriptorSets.size(); i++)
    {
        m_descriptorSets[i].reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
    }

    m_sampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1.0, VK_FILTER_LINEAR, 0.0f));
    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UBData)));

    Wolf::CreateImageInfo createImageInfo;
    createImageInfo.extent = { 2, 2, 2};
    createImageInfo.format = Wolf::Format::R32G32B32A32_SFLOAT;
    createImageInfo.mipLevelCount = 1;
    createImageInfo.usage = Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::TRANSFER_DST;
    m_defaultLUTImage.reset(Wolf::Image::createImage(createImageInfo));

    std::array<std::array<float, 4>, 8> defaultLUTData;
    for (uint32_t x = 0; x < 2; ++x)
    {
        for (uint32_t y = 0; y < 2; ++y)
        {
            for (uint32_t z = 0; z < 2; ++z)
            {
                defaultLUTData[x + y * 2 + z * 2 * 2] = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0.0f };
            }
        }
    }
    m_defaultLUTImage->copyCPUBuffer(reinterpret_cast<const unsigned char*>(defaultLUTData.data()),  { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT });

    m_swapChainImages = context.swapChainImages;
    m_uiImage = context.userInterfaceImage;
    updateDescriptorSets();

    // Load fullscreen rect
    const std::vector<Vertex2DTextured> vertices =
    {
        { glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 0.0f) }, // top left
        { glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 0.0f) }, // top right
        { glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 1.0f) }, // bot left
        { glm::vec2(1.0f, 1.0f),glm::vec2(1.0f, 1.0f) } // bot right
    };

    const std::vector<uint32_t> indices =
    {
        0, 2, 1,
        2, 3, 1
    };

    m_fullscreenRect.reset(new Wolf::Mesh(vertices, indices));

    Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
    switch (context.swapChainColorSpace)
    {
        case Wolf::SwapChain::SwapChainCreateInfo::ColorSpace::S_RGB:
            shaderCodeToAdd.codeString = "#define USE_S_RGB 1\n";
            break;
        case Wolf::SwapChain::SwapChainCreateInfo::ColorSpace::SC_RGB:
            shaderCodeToAdd.codeString = "#define USE_SC_RGB 1\n";
            break;
        case Wolf::SwapChain::SwapChainCreateInfo::ColorSpace::PQ:
            shaderCodeToAdd.codeString = "#define USE_PQ 1\n";
            break;
        case Wolf::SwapChain::SwapChainCreateInfo::ColorSpace::LINEAR:
            shaderCodeToAdd.codeString = "#define USE_LINEAR 1\n";
            break;
    }
    switch (context.swapChainImages[0]->getFormat())
    {
        case Wolf::Format::R8G8B8A8_UNORM:
            shaderCodeToAdd.codeString += "#define OUTPUT_FORMAT rgba8\n";
            break;
        case Wolf::Format::R16G16B16A16_SFLOAT:
            shaderCodeToAdd.codeString += "#define OUTPUT_FORMAT rgba16f\n";
            break;
        default:
            Wolf::Debug::sendError("Unsupported swap chain format");
    }
    m_computeShaderParser.reset(new Wolf::ShaderParser("Shaders/composition/shader.comp", {}, -1, -1, -1,
                                                       Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    createPipeline();
}

void CompositionPass::resize(const Wolf::InitializationContext& context)
{
    m_swapChainImages = context.swapChainImages;
    m_uiImage = context.userInterfaceImage;
    updateDescriptorSets();
    createPipeline();
}

void CompositionPass::record(const Wolf::RecordContext& context)
{
    PROFILE_FUNCTION

    const GameContext* gameContext = static_cast<const GameContext*>(context.gameContext);
    const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();

    UBData ubData{};
    ubData.displayType = static_cast<uint32_t>(gameContext->displayType);
    ubData.lutTexelSize = m_lutImage ? 1.0f / static_cast<float>(m_lutImage->getExtent().width) : 1.0f / static_cast<float>(m_defaultLUTImage->getExtent().width);
    ubData.renderOffsetX = renderViewport.x;
    ubData.renderOffsetY = renderViewport.y;
    ubData.renderSizeX = renderViewport.width;
    ubData.renderSizeY = renderViewport.height;
    m_uniformBuffer->transferCPUMemory(&ubData, sizeof(ubData));

    if (m_updateDescriptorSetRequested)
    {
        context.graphicAPIManager->waitIdle(); // unsure descriptor set is not used
        updateDescriptorSets();

        m_updateDescriptorSetRequested = false;
    }

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Composition Pass");

    context.swapchainImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });

    m_commandBuffer->bindPipeline(m_pipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_descriptorSets[context.swapChainImageIdx].createConstNonOwnerResource(), 0, *m_pipeline);

    constexpr Wolf::Extent3D dispatchGroups = { 16, 16, 1 };
    const uint32_t groupSizeX = context.swapchainImage->getExtent().width % dispatchGroups.width != 0 ? context.swapchainImage->getExtent().width / dispatchGroups.width + 1 : context.swapchainImage->getExtent().width / dispatchGroups.width;
    const uint32_t groupSizeY = context.swapchainImage->getExtent().height % dispatchGroups.height != 0 ? context.swapchainImage->getExtent().height / dispatchGroups.height + 1 : context.swapchainImage->getExtent().height / dispatchGroups.height;
    m_commandBuffer->dispatch(groupSizeX, groupSizeY, dispatchGroups.depth);

    context.swapchainImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_NONE, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT });

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_lastSwapChainImage = context.swapchainImage;
}

void CompositionPass::submit(const Wolf::SubmitContext& context)
{
    std::vector<const Wolf::Semaphore*> waitSemaphores{ context.swapChainImageAvailableSemaphore, context.userInterfaceImageAvailableSemaphore, m_forwardPass->getSemaphore(context.swapChainImageIndex) };

    const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, context.frameFence);

    bool anyShaderModified = m_computeShaderParser->compileIfFileHasBeenModified();
    if (anyShaderModified)
    {
        context.graphicAPIManager->waitIdle();
        createPipeline();
    }
}

void CompositionPass::clear()
{
    m_lutImage.release();
}

void CompositionPass::setInputLUT(const Wolf::ResourceNonOwner<Wolf::Image>& lutImage)
{
    m_lutImage = lutImage;
    m_updateDescriptorSetRequested = true;
}

void CompositionPass::releaseInputLUT()
{
    m_lutImage.release();
    m_updateDescriptorSetRequested = true;
}

void CompositionPass::saveSwapChainToFile()
{
    std::vector<uint8_t> fullImageData;
    m_lastSwapChainImage->exportToBuffer(fullImageData);

    uint32_t channelCount = 4;

    Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();
    renderViewport.x += 3;
    renderViewport.width -= 5;
    renderViewport.height -= 5;
    std::vector<uint8_t> renderImageData(renderViewport.width * renderViewport.height * channelCount);

    for (uint32_t y = 0; y < static_cast<uint32_t>(renderViewport.height); y++)
    {
        uint32_t offsetInFullImageData = ((y + static_cast<uint32_t>(renderViewport.y)) * m_lastSwapChainImage->getExtent().width * channelCount) + static_cast<uint32_t>(renderViewport.x) * channelCount;
        uint8_t* src = &fullImageData[offsetInFullImageData];
        uint8_t* dst = &renderImageData[y * renderViewport.width * channelCount];
        uint32_t lineCopySize = renderViewport.width * channelCount;

        memcpy(dst, src, lineCopySize);
    }

    for (uint32_t pixelIdx = 0; pixelIdx < renderViewport.width * renderViewport.height; pixelIdx++)
    {
        uint8_t r = renderImageData[4 * pixelIdx + 0];
        uint8_t g = renderImageData[4 * pixelIdx + 1];
        uint8_t b = renderImageData[4 * pixelIdx + 2];
        uint8_t a = renderImageData[4 * pixelIdx + 3];

        renderImageData[4 * pixelIdx + 0] = r;
        renderImageData[4 * pixelIdx + 1] = g;
        renderImageData[4 * pixelIdx + 2] = b;
        renderImageData[4 * pixelIdx + 3] = a;
    }

    stbi_write_png("screenshot.png", renderViewport.width, renderViewport.height, channelCount, renderImageData.data(), renderViewport.width * channelCount);
}

void CompositionPass::updateDescriptorSets()
{
    Wolf::DescriptorSetGenerator::ImageDescription forwardResultImageDesc;
    forwardResultImageDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    forwardResultImageDesc.imageView = m_forwardPass->getOutput().getDefaultImageView();

    Wolf::DescriptorSetGenerator::ImageDescription uiResultImageDesc;
    uiResultImageDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    uiResultImageDesc.imageView = m_uiImage->getDefaultImageView();

    for (uint32_t i = 0; i < m_descriptorSets.size(); i++)
    {
        Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
        descriptorSetGenerator.setImage(0, forwardResultImageDesc);
        descriptorSetGenerator.setImage(1, uiResultImageDesc);
        descriptorSetGenerator.setCombinedImageSampler(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_lutImage ? m_lutImage->getDefaultImageView() : m_defaultLUTImage->getDefaultImageView(), *m_sampler);
        descriptorSetGenerator.setUniformBuffer(3, *m_uniformBuffer);

        Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc;
        outputImageDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputImageDesc.imageView = m_swapChainImages[i]->getDefaultImageView();
        descriptorSetGenerator.setImage(4, outputImageDesc);

        m_descriptorSets[i]->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
    }
}

void CompositionPass::createPipeline()
{
    std::vector<char> computeShaderCode;
    m_computeShaderParser->readCompiledShader(computeShaderCode);

    Wolf::ShaderCreateInfo computeShaderCreateInfo;
    computeShaderCreateInfo.shaderCode = computeShaderCode;
    computeShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;

    std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts;
    descriptorSetLayouts.reserve(1);
    descriptorSetLayouts.emplace_back(m_descriptorSetLayout.createConstNonOwnerResource());

    m_pipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderCreateInfo, descriptorSetLayouts));
}