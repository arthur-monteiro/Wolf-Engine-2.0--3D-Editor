#include "HierarchicalZBufferBuildingPass.h"

#include <DebugMarker.h>
#include <DescriptorSetGenerator.h>
#include <MipMapGenerator.h>

HierarchicalZBufferBuildingPass::HierarchicalZBufferBuildingPass(const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<ForwardPass>& forwardPass, EditorParams* editorParams)
    : m_preDepthPass(preDepthPass), m_forwardPass(forwardPass), m_editorParams(editorParams)
{
}

void HierarchicalZBufferBuildingPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::COMPUTE, false, "HZB Building"));
    createSemaphores(context, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, false);

    m_descriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::COMPUTE, 0); // input depth
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::STORAGE_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE, 1, MAX_HZB_MIP_COUNT); // output mip
    m_descriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 2); // counter buffer
    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

    m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));

    m_sampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1.0, VK_FILTER_NEAREST, 0.0f));
    m_counterBuffer.reset(Wolf::Buffer::createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    m_computeShaderParser.reset(new Wolf::ShaderParser("Shaders/hzbBuilding/shader.comp"));

    createOutputImage();
    updateDescriptorSet();
    createPipeline();
}

void HierarchicalZBufferBuildingPass::resize(const Wolf::InitializationContext& context)
{
    createOutputImage();
    updateDescriptorSet();
}

void HierarchicalZBufferBuildingPass::record(const Wolf::RecordContext& context)
{
    PROFILE_FUNCTION

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "HZB Building Pass");

    m_commandBuffer->bindPipeline(m_pipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_descriptorSet.createConstNonOwnerResource(), 0, *m_pipeline);

    PushConstants pcData{};
    pcData.m_srcWidth = m_extent.width;
    pcData.m_srcHeight = m_extent.height;
    pcData.m_numMips = m_mipCount;
    pcData.m_workgroupSlice = 0;

    Wolf::Viewport viewport = m_editorParams->getRenderViewport();
    pcData.m_viewport = { viewport.x, viewport.y, viewport.width, viewport.height };

    m_commandBuffer->pushConstants(m_pipeline.createConstNonOwnerResource(), Wolf::ShaderStageFlagBits::COMPUTE, 0, sizeof(PushConstants), &pcData);

    constexpr Wolf::Extent3D dispatchGroups = { 16, 16, 1 };
    const uint32_t groupSizeX = context.m_swapchainImage->getExtent().width % dispatchGroups.width != 0 ? context.m_swapchainImage->getExtent().width / dispatchGroups.width + 1 : context.m_swapchainImage->getExtent().width / dispatchGroups.width;
    const uint32_t groupSizeY = context.m_swapchainImage->getExtent().height % dispatchGroups.height != 0 ? context.m_swapchainImage->getExtent().height / dispatchGroups.height + 1 : context.m_swapchainImage->getExtent().height / dispatchGroups.height;
    m_commandBuffer->dispatch(groupSizeX, groupSizeY, dispatchGroups.depth);

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    Wolf::Image::TransitionLayoutInfo transitionLayoutInfo;
    transitionLayoutInfo.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    transitionLayoutInfo.dstLayout = Wolf::ImageLayout::GENERAL;
    transitionLayoutInfo.dstPipelineStageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    transitionLayoutInfo.oldLayout = Wolf::ImageLayout::GENERAL;
    m_outputImage->transitionImageLayout(*m_commandBuffer, transitionLayoutInfo);

    m_commandBuffer->endCommandBuffer();
}

void HierarchicalZBufferBuildingPass::submit(const Wolf::SubmitContext& context)
{
    std::vector<const Wolf::Semaphore*> waitSemaphores{ m_forwardPass->getSemaphore(context.swapChainImageIndex) };

    const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);

    if (m_computeShaderParser->compileIfFileHasBeenModified())
    {
        context.graphicAPIManager->waitIdle();
        createPipeline();
    }
}

void HierarchicalZBufferBuildingPass::createOutputImage()
{
    m_extent = m_preDepthPass->getOutput()->getExtent();
    m_mipCount = Wolf::MipMapGenerator::computeMipCount(Wolf::Extent2D(m_extent.width, m_extent.height));
    if (m_mipCount > MAX_HZB_MIP_COUNT + 1)
    {
        Wolf::Debug::sendCriticalError("Max mip count reached (increase MAX_HZB_MIP_COUNT and in shader too)");
    }

    Wolf::CreateImageInfo createImageInfo{};
    createImageInfo.extent = Wolf::Extent3D{ m_extent.width >> 1, m_extent.height >> 1, 1 };
    createImageInfo.usage = Wolf::ImageUsageFlagBits::STORAGE | Wolf::ImageUsageFlagBits::SAMPLED;
    createImageInfo.format = Wolf::Format::R32_SFLOAT;
    createImageInfo.mipLevelCount = m_mipCount - 1;
    m_outputImage.reset(Wolf::Image::createImage(createImageInfo));

    m_outputImage->setImageLayout({ Wolf::ImageLayout::GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, createImageInfo.mipLevelCount,
        0, 1, Wolf::ImageLayout::UNDEFINED });
}

void HierarchicalZBufferBuildingPass::updateDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setCombinedImageSampler(0, Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL, m_preDepthPass->getOutput()->getDefaultImageView(), *m_sampler);

    std::vector<Wolf::DescriptorSetGenerator::ImageDescription> mipImageDescriptions;
    mipImageDescriptions.reserve(MAX_HZB_MIP_COUNT);
    for (uint32_t mipLevel = 1; mipLevel < m_mipCount; mipLevel++)
    {
        Wolf::DescriptorSetGenerator::ImageDescription& imageDescription = mipImageDescriptions.emplace_back();
        imageDescription.imageView = m_outputImage->getImageView(m_outputImage->getFormat(), mipLevel - 1, 1);
        imageDescription.imageLayout = Wolf::ImageLayout::GENERAL;
    }
    // Just add an image view for unused mips
    for (uint32_t mipLevel = m_mipCount; mipLevel < MAX_HZB_MIP_COUNT + 1; mipLevel++)
    {
        Wolf::DescriptorSetGenerator::ImageDescription& imageDescription = mipImageDescriptions.emplace_back();
        imageDescription.imageView = m_outputImage->getImageView(m_outputImage->getFormat(), 0, 1);
        imageDescription.imageLayout = Wolf::ImageLayout::GENERAL;
    }

    descriptorSetGenerator.setImages(1, mipImageDescriptions);
    descriptorSetGenerator.setBuffer(2, *m_counterBuffer);

    m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void HierarchicalZBufferBuildingPass::createPipeline()
{
    std::vector<char> computeShaderCode;
    m_computeShaderParser->readCompiledShader(computeShaderCode);

    Wolf::ShaderCreateInfo computeShaderCreateInfo;
    computeShaderCreateInfo.shaderCode = computeShaderCode;
    computeShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;

    std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts;
    descriptorSetLayouts.reserve(1);
    descriptorSetLayouts.emplace_back(m_descriptorSetLayout.createConstNonOwnerResource());

    std::vector<Wolf::PushConstantsRange> pushConstantsRanges(1);
    pushConstantsRanges[0] = { .m_offset = 0, .m_size = sizeof(PushConstants) };

    m_pipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderCreateInfo, descriptorSetLayouts, pushConstantsRanges));
}
