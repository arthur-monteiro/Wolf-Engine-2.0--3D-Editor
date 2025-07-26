#pragma once

#include <CommandRecordBase.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Image.h>
#include <Mesh.h>
#include <ResourceUniqueOwner.h>
#include <Sampler.h>
#include <ShaderBindingTable.h>
#include <ShaderParser.h>

#include "PreDepthPass.h"

class RayTracedWorldDebugPass : public Wolf::CommandRecordBase
{
public:
    RayTracedWorldDebugPass(const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass);

    void setTLAS(const Wolf::ResourceNonOwner<Wolf::TopLevelAccelerationStructure>& topLevelAccelerationStructure);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    bool wasEnabledThisFrame() const { return m_wasEnabledThisFrame;}
    void bindDescriptorSetToDrawOutput(const Wolf::CommandBuffer& commandBuffer, const Wolf::Pipeline& pipeline);

private:
    void createOutputImage(const Wolf::InitializationContext& context);
    void createRayTracingPipeline();
    void createRayTracingDescriptorSet();

    void createDrawRectPipelineIfNeeded(Wolf::RenderPass* renderPass);
    void createDrawRectDescriptorSet(const Wolf::InitializationContext& context);

    Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayGenShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_closestHitShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderBindingTable> m_shaderBindingTable;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_rayTracingPipeline;

    Wolf::DescriptorSetLayoutGenerator m_rayTracingDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_rayTracingDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_rayTracingDescriptorSet;

    Wolf::DescriptorSetLayoutGenerator m_drawRectDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_drawRectDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_drawRectDescriptorSet;
    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_drawRectSampler;

    bool m_wasEnabledThisFrame = false;

    Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
    Wolf::NullableResourceNonOwner<Wolf::TopLevelAccelerationStructure> m_topLevelAccelerationStructure;
};