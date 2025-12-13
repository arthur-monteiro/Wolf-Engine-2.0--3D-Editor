#pragma once

#include <cstdint>
#include <xxh64.hpp>

#include <ResourceUniqueOwner.h>

#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <LazyInitSharedResource.h>
#include <TopLevelAccelerationStructure.h>

#include <Mesh.h>
#include <ShaderParser.h>

class RayTracedWorldManager
{
public:
    RayTracedWorldManager();

    struct RayTracedWorldInfo
    {
        struct InstanceInfo
        {
            Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure> m_bottomLevelAccelerationStructure;
            glm::mat4 m_transform;

            uint32_t m_firstMaterialIdx;
            Wolf::ResourceNonOwner<Wolf::Mesh> m_mesh;

            Wolf::NullableResourceNonOwner<Wolf::Buffer> m_overrideIndexBuffer;
        };
        std::vector<InstanceInfo> m_instances;
    };
    void requestBuild(const RayTracedWorldInfo& info);
    void build(const Wolf::CommandBuffer& commandBuffer);
    void recordTLASBuildBarriers(const Wolf::CommandBuffer& commandBuffer);

    bool needsRebuildTLAS() const { return m_needsRebuildTLAS; }
    bool hasInstance() const { return static_cast<bool>(m_topLevelAccelerationStructure); };
    Wolf::ResourceNonOwner<const Wolf::DescriptorSet> getDescriptorSet() { return m_descriptorSet.createConstNonOwnerResource(); }
    static void addRayGenShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot);

    static Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& getDescriptorSetLayout() { return Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, RayTracedWorldManager>::getResource(); }

private:
    void requestBuildTLAS(const RayTracedWorldInfo& info);
    void populateInstanceBuffer(const RayTracedWorldInfo& info);
    void createDescriptorSet();
    uint32_t addStorageBuffer(const Wolf::ResourceNonOwner<Wolf::Buffer>& buffer);
    uint64_t computeBufferListHash() const;

    Wolf::ResourceUniqueOwner<Wolf::TopLevelAccelerationStructure> m_topLevelAccelerationStructure;

    struct InstanceData
    {
        uint32_t firstMaterialIdx;
        uint32_t vertexBufferBindlessOffset;
        uint32_t indexBufferBindlessOffset;
    };
    static constexpr uint32_t MAX_INSTANCES = 16384;
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_instanceBuffer;

    Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;;
    Wolf::ResourceUniqueOwner<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, RayTracedWorldManager>> m_descriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

    std::vector<Wolf::ResourceNonOwner<Wolf::Buffer>> m_buffers;
    uint32_t m_currentBindlessCount = 0;
    uint64_t m_buffersListHash = 0;

    std::vector<Wolf::BLASInstance> m_blasInstances;
    std::vector<InstanceData> m_instanceData;
    bool m_needsRebuildTLAS =false;
};
