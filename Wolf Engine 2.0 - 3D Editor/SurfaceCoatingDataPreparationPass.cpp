#include "SurfaceCoatingDataPreparationPass.h"

#include <DebugMarker.h>
#include <DescriptorSetGenerator.h>

#include "SurfaceCoatingEmitterComponent.h"

SurfaceCoatingDataPreparationPass::SurfaceCoatingDataPreparationPass(const Wolf::ResourceNonOwner<CustomSceneRenderPass>& customSceneRenderPass)
    : m_customSceneRenderPass(customSceneRenderPass)
{
}

void SurfaceCoatingDataPreparationPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::COMPUTE, false));
    createSemaphores(context, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, false);

    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE, 0, 1); // input depth
    m_descriptorSetLayoutGenerator.addSampler(Wolf::ShaderStageFlagBits::COMPUTE, 1); // input depth
    m_descriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 2); // uniform buffer
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::STORAGE_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE, 3, 1); // output image
    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

    m_computeShaderParser.reset(new Wolf::ShaderParser("Shaders/surfaceCoatingDataPreparation/shader.comp", {}, -1, -1, -1));
    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));
    createPipeline();
}

void SurfaceCoatingDataPreparationPass::resize(const Wolf::InitializationContext& context)
{
    // Nothing to do
}

void SurfaceCoatingDataPreparationPass::record(const Wolf::RecordContext& context)
{
    if (!m_component)
    {
        m_commandsRecordedThisFrame = false;
        return;
    }

    UniformBufferData uniformBufferData{};
    uniformBufferData.transform = m_component->computeTransform();
    uniformBufferData.depthViewProjMatrix = m_component->getDepthViewProj();

    Wolf::Extent3D depthImageSize = m_component->getDepthImage()->getExtent();
    uniformBufferData.depthImageSize = glm::vec2(depthImageSize.width, depthImageSize.height);

    m_uniformBuffer->transferCPUMemory(&uniformBufferData, sizeof(UniformBufferData));

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Surface coating data preparation pass");

    m_commandBuffer->bindPipeline(m_pipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_descriptorSet.createConstNonOwnerResource(), 0, *m_pipeline);

    constexpr Wolf::Extent3D dispatchGroups = { 16, 16, 1 };
    const uint32_t groupSizeX = 32 % dispatchGroups.width != 0 ? 32 / dispatchGroups.width + 1 : 32 / dispatchGroups.width;
    const uint32_t groupSizeY = 32 % dispatchGroups.height != 0 ? 32 / dispatchGroups.height + 1 : 32 / dispatchGroups.height;
    m_commandBuffer->dispatch(groupSizeX, groupSizeY, 1);

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_commandsRecordedThisFrame = true;
}

void SurfaceCoatingDataPreparationPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_commandsRecordedThisFrame)
        return;

    std::vector<const Wolf::Semaphore*> waitSemaphores{ };
    if (m_customSceneRenderPass->commandsRecordedThisFrame())
    {
        waitSemaphores.push_back(m_customSceneRenderPass->getSemaphore(context.swapChainImageIndex));
    }

    const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
}

void SurfaceCoatingDataPreparationPass::registerComponent(SurfaceCoatingEmitterComponent* component)
{
    if (m_component != nullptr)
    {
        Wolf::Debug::sendError("A component was already registered");
    }
    m_component = component;
    createOrUpdateDescriptorSet();
}

void SurfaceCoatingDataPreparationPass::unregisterComponent(const SurfaceCoatingEmitterComponent* component)
{
    if (m_component != component)
    {
        Wolf::Debug::sendError("This is not the component registered");
    }
    m_component = nullptr;
}

void SurfaceCoatingDataPreparationPass::createPipeline()
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

void SurfaceCoatingDataPreparationPass::createOrUpdateDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());

    Wolf::DescriptorSetGenerator::ImageDescription inputDepthImageDesc{};
    inputDepthImageDesc.imageLayout = Wolf::ImageLayout::GENERAL;
    inputDepthImageDesc.imageView = m_component->getDepthImage()->getDefaultImageView();
    descriptorSetGenerator.setImage(0, inputDepthImageDesc);

    descriptorSetGenerator.setSampler(1, *m_component->getSampler());
    descriptorSetGenerator.setUniformBuffer(2, *m_uniformBuffer);

    Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc{};
    outputImageDesc.imageLayout = Wolf::ImageLayout::GENERAL;
    outputImageDesc.imageView = m_component->getPatchBoundsImage()->getDefaultImageView();
    descriptorSetGenerator.setImage(3, outputImageDesc);

    if (!m_descriptorSet)
    {
        m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
    }
    m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}
