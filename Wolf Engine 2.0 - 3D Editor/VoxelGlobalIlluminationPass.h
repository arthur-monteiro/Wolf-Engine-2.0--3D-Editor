#pragma once

#include <CommandRecordBase.h>
#include <Image.h>
#include <ResourceUniqueOwner.h>
#include <UniformBuffer.h>

#include "GlobalIrradiancePassInterface.h"
#include "RayTracedWorldManager.h"
#include "ResourceManager.h"

class VoxelGlobalIlluminationPass : public Wolf::CommandRecordBase, public GlobalIrradiancePassInterface
{
public:
    VoxelGlobalIlluminationPass(const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager);

    void setResourceManager(const Wolf::ResourceNonOwner<ResourceManager>& resourceManager);
    void setEnableDebug(bool value) { m_enableDebug = value; }
    void addMeshesToRenderList(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    [[nodiscard]] bool wasEnabledThisFrame() const override { return m_wasEnabledThisFrame; }
    [[nodiscard]] const Wolf::Semaphore* getSemaphore() const override { return Wolf::CommandRecordBase::getSemaphore(0 /* as this is never used as last semaphore it should be ok */); }
    void addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const override;

private:
    void createVoxelGrid();
    void initGridUpdateShaders();
    void createPipeline();
    void createDescriptorSet();
    void createNoiseImage();

    void initDebugResources();
    void initializeDebugPipelineSet();

    void createOutputDescriptorSet();

    Wolf::ResourceNonOwner<RayTracedWorldManager> m_rayTracedWorldManager;
    Wolf::NullableResourceNonOwner<ResourceManager> m_resourceManager;

    bool m_wasEnabledThisFrame = false;

    static constexpr uint32_t GRID_SIZE = 32;
    struct VoxelLayout
    {
        glm::vec3 irradiance[6];
    };
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_voxelGrid;

    struct RequestVoxelLayout
    {
        uint32_t requests; // each bit is a face
    };
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_requestsBuffer;

    // Grid update
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayGenShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_closestHitShaderParser;
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

    ResourceManager::ResourceId m_sphereMeshResourceId = ResourceManager::NO_RESOURCE;
    struct SphereInstanceData
    {
        glm::vec3 worldPos;

        static void getBindingDescription(VkVertexInputBindingDescription& bindingDescription, uint32_t binding)
        {
            bindingDescription.binding = binding;
            bindingDescription.stride = sizeof(SphereInstanceData);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        }

        static void getAttributeDescriptions(std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, uint32_t binding, uint32_t startLocation)
        {
            const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
            attributeDescriptions.resize(attributeDescriptionCountBefore + 1);

            attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
            attributeDescriptions[attributeDescriptionCountBefore + 0].location = startLocation;
            attributeDescriptions[attributeDescriptionCountBefore + 0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(SphereInstanceData, worldPos);
        }
    };
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_sphereInstanceBuffer;
    Wolf::ResourceUniqueOwner<Wolf::PipelineSet> m_debugPipelineSet;

    Wolf::DescriptorSetLayoutGenerator m_debugDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_debugDescriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_debugDescriptorSet;

    inline static const glm::vec3 FIRST_PROBE_POS{ -10.0f, -5.0f, -10.0f};
    inline static constexpr float SPACE_BETWEEN_PROBES = 1.0f;
};
