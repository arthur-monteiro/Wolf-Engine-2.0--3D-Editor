#include "DrawRectInterface.h"

#include <DescriptorSetGenerator.h>
#include <DescriptorSetLayout.h>
#include <RenderPass.h>

#include "Vertex2DTextured.h"

void DrawRectInterface::bindInfoToDrawOutput(const Wolf::CommandBuffer& commandBuffer, const Wolf::RenderPass* renderPass)
{
    createPipeline(renderPass);

    commandBuffer.bindPipeline(m_drawRectPipeline.createConstNonOwnerResource());
    commandBuffer.bindDescriptorSet(m_drawRectDescriptorSet.createConstNonOwnerResource(), 0, *m_drawRectPipeline);
}

void DrawRectInterface::initializeResources(const Wolf::InitializationContext& context)
{
    m_drawRectDescriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::FRAGMENT, 0);

    m_drawRectDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_drawRectDescriptorSetLayoutGenerator.getDescriptorLayouts()));
    m_drawRectSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR));
    createDescriptorSet(context);

    m_drawRectVertexShaderParser.reset(new Wolf::ShaderParser("Shaders/drawRect/drawRect.vert"));
    m_drawRectFragmentShaderParser.reset(new Wolf::ShaderParser("Shaders/drawRect/drawRect.frag"));
}

void DrawRectInterface::createDescriptorSet(const Wolf::InitializationContext& context)
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_drawRectDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, getOutputImage()->getDefaultImageView(), *m_drawRectSampler);

    if (!m_drawRectDescriptorSet)
        m_drawRectDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_drawRectDescriptorSetLayout));
    m_drawRectDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void DrawRectInterface::createPipeline(const Wolf::RenderPass* renderPass)
{
    if (renderPass == m_lastRegisteredRenderPass)
        return;

    m_lastRegisteredRenderPass = renderPass;

    Wolf::RenderingPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.renderPass = renderPass;

    // Programming stages
    pipelineCreateInfo.shaderCreateInfos.resize(2);
    m_drawRectVertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
    pipelineCreateInfo.shaderCreateInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;
    m_drawRectFragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
    pipelineCreateInfo.shaderCreateInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

    // IA
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    Vertex2DTextured::getAttributeDescriptions(attributeDescriptions, 0);
    pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0] = {};
    Vertex2DTextured::getBindingDescription(bindingDescriptions[0], 0);
    pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

    // Viewport
    pipelineCreateInfo.extent = { renderPass->getExtent().width, renderPass->getExtent().height };

    // Depth
    pipelineCreateInfo.enableDepthWrite = false;

    // Resources
    pipelineCreateInfo.descriptorSetLayouts = { m_drawRectDescriptorSetLayout.createConstNonOwnerResource() };

    // Dynamic state
    pipelineCreateInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

    // Color Blend
    pipelineCreateInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ALPHA };

    m_drawRectPipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));
}
