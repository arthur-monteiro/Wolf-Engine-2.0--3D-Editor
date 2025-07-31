#include "RayTracedWorldManager.h"

#include <Buffer.h>
#include <DescriptorSetLayoutGenerator.h>

RayTracedWorldManager::RayTracedWorldManager()
{
    m_instanceBuffer.reset(Wolf::Buffer::createBuffer(MAX_INSTANCES * sizeof(InstanceData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    m_descriptorSetLayoutGenerator.reset();
    m_descriptorSetLayoutGenerator.addStorageBuffers(Wolf::ShaderStageFlagBits::CLOSEST_HIT, 0, MAX_INSTANCES, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
    m_descriptorSetLayoutGenerator.addAccelerationStructure(Wolf::ShaderStageFlagBits::RAYGEN, 1);

    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT));
    m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
}

void RayTracedWorldManager::build(const RayTracedWorldInfo& info)
{
    if (info.m_instances.size() > MAX_INSTANCES)
    {
        Wolf::Debug::sendCriticalError("Too much BLAS instances, instance buffer will be too small");
    }

    buildTLAS(info);
    populateInstanceBuffer(info);
}

void RayTracedWorldManager::buildTLAS(const RayTracedWorldInfo& info)
{
    std::vector<Wolf::BLASInstance> blasInstances(info.m_instances.size());
    for (size_t i = 0; i < blasInstances.size(); i++)
    {
        blasInstances[i].bottomLevelAS = info.m_instances[i].m_bottomLevelAccelerationStructure.operator->();
        blasInstances[i].transform = info.m_instances[i].m_transform;
        blasInstances[i].instanceID = i;
        blasInstances[i].hitGroupIndex = 0;
    }

    m_topLevelAccelerationStructure.reset(Wolf::TopLevelAccelerationStructure::createTopLevelAccelerationStructure(blasInstances));
}

void RayTracedWorldManager::populateInstanceBuffer(const RayTracedWorldInfo& info)
{
    std::vector<InstanceData> instanceData(info.m_instances.size());
    for (size_t i = 0; i < instanceData.size(); i++)
    {
        InstanceData& data = instanceData[i];
        const RayTracedWorldInfo::InstanceInfo& instanceInfo = info.m_instances[i];

        data.firstMaterialIdx = instanceInfo.m_firstMaterialIdx;
        data.vertexBufferBindlessOffset = addStorageBufferToBindless(*instanceInfo.m_mesh->getVertexBuffer());
        data.indexBufferBindlessOffset = addStorageBufferToBindless(instanceInfo.m_mesh->getIndexBuffer());
    }

    m_instanceBuffer->transferCPUMemoryWithStagingBuffer(instanceData.data(), instanceData.size() * sizeof(InstanceData));
}

uint32_t RayTracedWorldManager::addStorageBufferToBindless(const Wolf::Buffer& buffer)
{
    if (m_currentBindlessCount >= MAX_INSTANCES)
    {
        Wolf::Debug::sendError("Using more slots than specified at descriptor set layout creation, does it cause a crash?");
    }

    const uint32_t previousCounter = m_currentBindlessCount;

    Wolf::DescriptorSetUpdateInfo descriptorSetUpdateInfo;
    descriptorSetUpdateInfo.descriptorBuffers.resize(1);
    descriptorSetUpdateInfo.descriptorBuffers[0].buffers.resize(1);
    descriptorSetUpdateInfo.descriptorBuffers[0].buffers[0] = &buffer;

    {
        Wolf::DescriptorLayout& descriptorLayout = descriptorSetUpdateInfo.descriptorBuffers[0].descriptorLayout;
        descriptorLayout.binding = 0;
        descriptorLayout.arrayIndex = m_currentBindlessCount++;
        descriptorLayout.descriptorType = Wolf::DescriptorType::STORAGE_BUFFER;
    }

    if (m_currentBindlessCount == 0)
    {
        descriptorSetUpdateInfo.descriptorAccelerationStructures.resize(1);
        descriptorSetUpdateInfo.descriptorAccelerationStructures[0].accelerationStructure = m_topLevelAccelerationStructure.operator->();

        Wolf::DescriptorLayout& descriptorLayout = descriptorSetUpdateInfo.descriptorAccelerationStructures[0].descriptorLayout;
        descriptorLayout.binding = 1;
        descriptorLayout.descriptorType = Wolf::DescriptorType::ACCELERATION_STRUCTURE;
    }

    m_descriptorSet->update(descriptorSetUpdateInfo);

    return previousCounter;
}