#pragma once

#include <CommandRecordBase.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Image.h>
#include <Mesh.h>
#include <ResourceUniqueOwner.h>
#include <Sampler.h>
#include <ShaderBindingTable.h>
#include <ShaderParser.h>
#include <UniformBuffer.h>

#include "DrawRectInterface.h"
#include "PreDepthPass.h"
#include "RayTracedWorldManager.h"
#include "UpdateRayTracedWorldPass.h"

class RayTracedWorldDebugPass : public Wolf::CommandRecordBase, public DrawRectInterface
{
public:
    RayTracedWorldDebugPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<UpdateRayTracedWorldPass>& updateRayTracedWorldPass,
        const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    [[nodiscard]] bool wasEnabledThisFrame() const { return m_wasEnabledThisFrame; }

private:
    Wolf::ResourceNonOwner<UpdateRayTracedWorldPass> m_updateRayTracedWorldPass;

    Wolf::ResourceUniqueOwner<Wolf::Image>& getOutputImage() override { return m_outputImage;}

    void createOutputImage(const Wolf::InitializationContext& context);
    void createPipeline();
    void createDescriptorSet();

    Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;
    Wolf::ResourceNonOwner<RayTracedWorldManager> m_rayTracedWorldManager;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayGenShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_closestHitShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderBindingTable> m_shaderBindingTable;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_pipeline;

    struct UniformBufferData
    {
        uint32_t displayType;
        uint32_t screenOffsetX;
        uint32_t screenOffsetY;
        uint32_t padding;
    };
    std::unique_ptr<Wolf::UniformBuffer> m_displayOptionsUniformBuffer;
    Wolf::DescriptorSetLayoutGenerator m_rayTracingDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_rayTracingDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_rayTracingDescriptorSet;

    bool m_wasEnabledThisFrame = false;

    Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
    EditorParams* m_editorParams;
};
