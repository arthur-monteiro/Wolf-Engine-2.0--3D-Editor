#pragma once

#include <ResourceUniqueOwner.h>

#include <CommandRecordBase.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Sampler.h>
#include <ShaderParser.h>


class DrawRectInterface
{
public:
    void bindInfoToDrawOutput(const Wolf::CommandBuffer& commandBuffer, const Wolf::RenderPass* renderPass);

protected:
    virtual Wolf::ResourceUniqueOwner<Wolf::Image>& getOutputImage() = 0;

    void initializeResources(const Wolf::InitializationContext& context);
    void createDescriptorSet(const Wolf::InitializationContext& context);

private:
    void createPipeline(const Wolf::RenderPass* renderPass);

    Wolf::DescriptorSetLayoutGenerator m_drawRectDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_drawRectDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_drawRectDescriptorSet;
    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_drawRectSampler;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_drawRectVertexShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_drawRectFragmentShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_drawRectPipeline;

    const Wolf::RenderPass* m_lastRegisteredRenderPass = nullptr;
};
