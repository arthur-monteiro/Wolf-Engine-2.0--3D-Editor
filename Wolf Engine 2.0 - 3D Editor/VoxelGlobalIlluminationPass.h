#pragma once

#include <CommandRecordBase.h>
#include <Image.h>
#include <ResourceUniqueOwner.h>
#include <UniformBuffer.h>

#include "GlobalIrradiancePassInterface.h"
#include "RayTracedWorldManager.h"
#include "AssetManager.h"
#include "UpdateRayTracedWorldPass.h"

class VoxelGlobalIlluminationPass : public Wolf::CommandRecordBase, public GlobalIrradiancePassInterface
{
public:
    VoxelGlobalIlluminationPass(const Wolf::ResourceNonOwner<UpdateRayTracedWorldPass>& updateRayTracedWorldPass, const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager);

    void setResourceManager(const Wolf::ResourceNonOwner<AssetManager>& resourceManager);
    void setEnableDebug(bool value) { m_enableDebug = value; }
    void setDebugPostionFace(uint32_t faceId) { m_debugPositionFaceId = faceId;}
    void addMeshesToRenderList(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    [[nodiscard]] bool wasEnabledThisFrame() const override { return m_wasEnabledThisFrame; }
    [[nodiscard]] const Wolf::Semaphore* getSemaphore() const override { return Wolf::CommandRecordBase::getSemaphore(0 /* as this is never used as last semaphore it should be ok */); }
    void addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const override;

private:
    Wolf::ResourceNonOwner<UpdateRayTracedWorldPass> m_updateRayTracedWorldPass;

    void createVoxelGrid();
    void initGridUpdateShaders();
    void createPipeline();
    void createDescriptorSet();
    void createNoiseImage();

    void initDebugResources();
    void initializeDebugPipelineSet();

    void createOutputDescriptorSet();

    Wolf::ResourceNonOwner<RayTracedWorldManager> m_rayTracedWorldManager;
    Wolf::NullableResourceNonOwner<AssetManager> m_resourceManager;

    bool m_wasEnabledThisFrame = false;

    static constexpr uint32_t GRID_SIZE = 32;
    struct VoxelLayout
    {
        glm::vec3 irradiance[6];
        glm::vec3 computePositions[6];
    };
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_voxelGrid;

    struct RequestVoxelLayout
    {
        uint32_t requests; // each bit is a face
    };
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_requestsBuffer;
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_requestsBufferCopy;

    // Grid update
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayGenShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_closestHitShaderParser;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_shadowMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_shadowClosestHitShaderParser;

    Wolf::ResourceUniqueOwner<Wolf::ShaderBindingTable> m_shaderBindingTable;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_pipeline;

    Wolf::DescriptorSetLayoutGenerator m_rayTracingDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_rayTracingDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_rayTracingDescriptorSet;

    struct UniformBufferData
    {
        uint32_t frameIdx;
        glm::vec3 padding;
    };
    Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;

    static constexpr uint32_t NOISE_TEXTURE_SIZE_PER_SIDE = 128;
    static constexpr uint32_t NOISE_TEXTURE_VECTOR_COUNT = 128;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_noiseImage;
    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_noiseSampler;

    // Debug
    bool m_enableDebug = false;
    uint32_t m_debugPositionFaceId = 0; // probe center

    AssetId m_sphereMeshResourceId = NO_ASSET;
    Wolf::ResourceUniqueOwner<Wolf::PipelineSet> m_debugPipelineSet;

    Wolf::DescriptorSetLayoutGenerator m_debugDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_debugDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_debugDescriptorSet;

    struct DebugUniformBufferData
    {
        uint32_t debugPositionFaceId;
    };
    Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_debugUniformBuffer;

    inline static const glm::vec3 FIRST_PROBE_POS{ -16.0f, -5.0f, -16.0f};
    inline static constexpr float SPACE_BETWEEN_PROBES = 1.0f;
};
