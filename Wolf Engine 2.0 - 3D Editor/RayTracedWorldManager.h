#pragma once

#include <cstdint>

#include <ResourceUniqueOwner.h>

#include <DescriptorSet.h>
#include <DescriptorSetGenerator.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <TopLevelAccelerationStructure.h>

#include <Mesh.h>

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
        };
        std::vector<InstanceInfo> m_instances;
    };
    void build(const RayTracedWorldInfo& info);

    // TODO: get descriptor set only and add a function to add shader code
    Wolf::ResourceNonOwner<Wolf::TopLevelAccelerationStructure> getTopLevelAccelerationStructure() { return m_topLevelAccelerationStructure.createNonOwnerResource(); }
    Wolf::ResourceNonOwner<Wolf::Buffer> getInstanceBuffer() { return m_instanceBuffer.createNonOwnerResource(); }

private:
    void buildTLAS(const RayTracedWorldInfo& info);
    void populateInstanceBuffer(const RayTracedWorldInfo& info);
    uint32_t addStorageBufferToBindless(const Wolf::Buffer& buffer);

    Wolf::ResourceUniqueOwner<Wolf::TopLevelAccelerationStructure> m_topLevelAccelerationStructure;

    struct InstanceData
    {
        uint32_t firstMaterialIdx;
        uint32_t vertexBufferBindlessOffset;
        uint32_t indexBufferBindlessOffset;
    };
    static constexpr uint32_t MAX_INSTANCES = 1024;
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_instanceBuffer;

    Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
    uint32_t m_currentBindlessCount = 0;
};
