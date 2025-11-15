#pragma once

#include <CommandRecordBase.h>
#include <UniformBuffer.h>

#include "ComputeSkyCubeMapPass.h"
#include "DrawRectInterface.h"
#include "PreDepthPass.h"
#include "RayTracedWorldManager.h"

class PathTracingPass : public Wolf::CommandRecordBase, public DrawRectInterface
{
public:
    PathTracingPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<ComputeSkyCubeMapPass>& computeSkyCubeMapPass,
        const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    [[nodiscard]] bool wasEnabledThisFrame() const { return m_wasEnabledThisFrame; }

private:
    Wolf::ResourceUniqueOwner<Wolf::Image>& getOutputImage() override { return m_outputImage;}

    void createImages(const Wolf::InitializationContext& context);
    void createNoiseImage();
    void createPipeline();
    void createDescriptorSet();

    uint64_t computeInfoHash(const glm::mat4& viewMatrix) const;
    void initImagesContent(uint64_t infoHash, const std::string& currentSceneName);

    EditorParams* m_editorParams;
    Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
    Wolf::ResourceNonOwner<ComputeSkyCubeMapPass> m_computeSkyCubeMapPass;
    Wolf::ResourceNonOwner<RayTracedWorldManager> m_rayTracedWorldManager;

    Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_inputImage;
    bool m_isFirstRecord = true;
    bool m_wasEnabledThisFrame = false;

    static constexpr uint32_t DITHER_SIZE = 4;
    uint32_t m_ditherIdx = 0;
    uint32_t m_internalFrameIdx = 0;
    uint64_t m_previousInfoHash = 0ul;
    bool m_hasInfoChangedFromInit = false;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayGenShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_closestHitShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderBindingTable> m_shaderBindingTable;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_pipeline;

    Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

    // Noise
    static constexpr uint32_t NOISE_TEXTURE_SIZE_PER_SIDE = 128;
    static constexpr uint32_t NOISE_TEXTURE_VECTOR_COUNT = 128;
    static constexpr uint32_t MAX_BOUNCE_COUNT = 4;
    static constexpr uint32_t NOISE_VECTOR_COUNT_PER_BOUNCE = NOISE_TEXTURE_VECTOR_COUNT / MAX_BOUNCE_COUNT;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_noiseImage;
    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_noiseSampler;

    struct UniformBufferData
    {
        uint32_t noiseIdx;
        uint32_t ditherX;
        uint32_t ditherY;
        uint32_t frameIdx;

        uint32_t screenOffsetX;
        uint32_t screenOffsetY;
        glm::uvec2 padding;
    };
    Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;
};
