#pragma once

#include <CommandRecordBase.h>
#include <UniformBuffer.h>

#include "EditorParams.h"
#include "GPUNoiseManager.h"
#include "PreDepthPass.h"
#include "RayTracedWorldManager.h"
#include "ShadowMaskPassInterface.h"
#include "UpdateRayTracedWorldPass.h"

class RayTracedShadowsPass : public Wolf::CommandRecordBase, public ShadowMaskPassInterface
{
public:
    RayTracedShadowsPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<UpdateRayTracedWorldPass>& updateRayTracedWorldPass,
        const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager, const Wolf::ResourceNonOwner<GPUNoiseManager>& noiseManager);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    void addComputeShadowsShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const override;

    Wolf::Image* getOutput() override { return m_shadowMaskImage0.get(); }
    Wolf::Semaphore* getSemaphore() const override { return Wolf::CommandRecordBase::getSemaphore(0 /* as this is never used as last semaphore it should be ok */); }
    void getConditionalBlocksToEnableWhenReadingMask(std::vector<std::string>& conditionalBlocks) const override {}
    Wolf::Image* getDenoisingPatternImage() override { return nullptr; }

    bool wasEnabledThisFrame() const override { return m_wasEnabledThisFrame; }

private:
    void createOutputImages(const Wolf::InitializationContext& context);
    void createPipelines();
    void createDescriptorSets();

    EditorParams* m_editorParams;
    Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
    Wolf::ResourceNonOwner<UpdateRayTracedWorldPass> m_updateRayTracedWorldPass;
    Wolf::ResourceNonOwner<RayTracedWorldManager> m_rayTracedWorldManager;
    Wolf::ResourceNonOwner<GPUNoiseManager> m_noiseManager;

    std::unique_ptr<Wolf::Image> m_shadowMaskImage0;
    std::unique_ptr<Wolf::Image> m_shadowMaskImage1;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_occluderDistanceImage;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayGenShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_closestHitShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderBindingTable> m_shaderBindingTable;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_rayTracingPipeline;

    struct RayTracingUniformBufferData
    {
        glm::vec4 sunDirectionAndNoiseIndex;

        float sunAreaAngle;
        uint32_t screenOffsetX;
        uint32_t screenOffsetY;
        float padding;
    };
    std::unique_ptr<Wolf::UniformBuffer> m_rayTracingUniformBuffer;
    Wolf::DescriptorSetLayoutGenerator m_rayTracingDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_rayTracingDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_rayTracingDescriptorSet;

    /* Denoising */

    // Penumbra scale compute
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_penumbraScaleComputeShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_penumbraScaleComputePipeline;

    struct PenumbraScaleComputeUniformBuffer
    {
        float m_sunAreaAngle;
        uint32_t m_screenHeight;
        float m_tanHalfFov;
    };
    Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_penumbraScaleComputeUniformBuffer;
    Wolf::DescriptorSetLayoutGenerator m_penumbraScaleComputeDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_penumbraScaleComputeDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_penumbraScaleComputeDescriptorSet;

    Wolf::ResourceUniqueOwner<Wolf::Image> m_penumbraScaleImage;

    // Blur
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_denoiseHorizontalBlurShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_denoiseVerticalBlurShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_denoiseHorizontalBlurPipeline;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_denoiseVerticalBlurPipeline;

    Wolf::DescriptorSetLayoutGenerator m_denoiseBlurDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_denoiseBlurDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_denoiseFirstDescriptorSet;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_denoiseSecondDescriptorSet;

    bool m_wasEnabledThisFrame = false;
};
