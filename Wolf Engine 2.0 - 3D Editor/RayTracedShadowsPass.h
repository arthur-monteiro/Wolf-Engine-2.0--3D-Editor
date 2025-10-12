#pragma once

#include <CommandRecordBase.h>
#include <UniformBuffer.h>

#include "EditorParams.h"
#include "PreDepthPass.h"
#include "RayTracedWorldManager.h"
#include "ShadowMaskPassInterface.h"

class RayTracedShadowsPass : public Wolf::CommandRecordBase, public ShadowMaskPassInterface
{
public:
    RayTracedShadowsPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    void addComputeShadowsShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const override;

    Wolf::Image* getOutput() override { return m_outputImage.get(); }
    Wolf::Semaphore* getSemaphore() const override { return Wolf::CommandRecordBase::getSemaphore(0 /* as this is never used as last semaphore it should be ok */); }
    void getConditionalBlocksToEnableWhenReadingMask(std::vector<std::string>& conditionalBlocks) const override {}
    Wolf::Image* getDenoisingPatternImage() override { return nullptr; }

    bool wasEnabledThisFrame() const override { return m_wasEnabledThisFrame; }

private:
    void createOutputImage(const Wolf::InitializationContext& context);
    void createPipeline();
    void createDescriptorSet();

    EditorParams* m_editorParams;
    Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
    Wolf::ResourceNonOwner<RayTracedWorldManager> m_rayTracedWorldManager;

    std::unique_ptr<Wolf::Image> m_outputImage;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayGenShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_closestHitShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderBindingTable> m_shaderBindingTable;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_pipeline;

    struct UniformBufferData
    {
        glm::vec4 sunDirectionAndNoiseIndex;

        float sunAreaAngle;
        uint32_t screenOffsetX;
        uint32_t screenOffsetY;
        float padding;
    };
    std::unique_ptr<Wolf::UniformBuffer> m_uniformBuffer;
    Wolf::DescriptorSetLayoutGenerator m_rayTracingDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_rayTracingDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_rayTracingDescriptorSet;

    bool m_wasEnabledThisFrame = false;
};
