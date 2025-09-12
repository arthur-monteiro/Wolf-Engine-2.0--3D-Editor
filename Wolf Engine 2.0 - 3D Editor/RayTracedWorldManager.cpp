#include "RayTracedWorldManager.h"

#include <Buffer.h>
#include <DescriptorSetGenerator.h>
#include <DescriptorSetLayoutGenerator.h>
#include <fstream>

RayTracedWorldManager::RayTracedWorldManager()
{
    m_instanceBuffer.reset(Wolf::Buffer::createBuffer(MAX_INSTANCES * sizeof(InstanceData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    m_descriptorSetLayoutGenerator.reset();
    m_descriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::RAYGEN | Wolf::ShaderStageFlagBits::CLOSEST_HIT, 0); // instance buffer
    m_descriptorSetLayoutGenerator.addAccelerationStructure(Wolf::ShaderStageFlagBits::RAYGEN | Wolf::ShaderStageFlagBits::CLOSEST_HIT, 1); // TLAS
    m_descriptorSetLayoutGenerator.addStorageBuffers(Wolf::ShaderStageFlagBits::RAYGEN | Wolf::ShaderStageFlagBits::CLOSEST_HIT, 2,
        2 * MAX_INSTANCES, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT); // Vertex and indices buffers

    m_descriptorSetLayout.reset(new Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, RayTracedWorldManager>([this](Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& descriptorSetLayout)
    {
        descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts(),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT));
    }));

    m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout->getResource()));
}

void RayTracedWorldManager::build(const RayTracedWorldInfo& info)
{
    if (info.m_instances.size() > MAX_INSTANCES)
    {
        Wolf::Debug::sendCriticalError("Too much BLAS instances, instance buffer will be too small");
    }
    m_buffers.clear();

    buildTLAS(info);
    populateInstanceBuffer(info);
    createDescriptorSet();
}

void RayTracedWorldManager::addRayGenShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const
{
    std::ifstream inFile("Shaders/rayTracedWorld/rayGen.glsl");
    std::string line;
    while (std::getline(inFile, line))
    {
        const std::string& descriptorSlotToken = "@RAY_TRACING_DESCRIPTOR_SLOT";
        size_t descriptorSlotTokenPos = line.find(descriptorSlotToken);
        while (descriptorSlotTokenPos != std::string::npos)
        {
            line.replace(descriptorSlotTokenPos, descriptorSlotToken.length(), std::to_string(bindingSlot));
            descriptorSlotTokenPos = line.find(descriptorSlotToken);
        }
        inOutShaderCodeToAdd.codeString += line + '\n';
    }
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
        data.vertexBufferBindlessOffset = addStorageBuffer(instanceInfo.m_mesh->getVertexBuffer());
        data.indexBufferBindlessOffset = addStorageBuffer(instanceInfo.m_mesh->getIndexBuffer());
    }

    m_instanceBuffer->transferCPUMemoryWithStagingBuffer(instanceData.data(), instanceData.size() * sizeof(InstanceData));
}

void RayTracedWorldManager::createDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setBuffer(0, *m_instanceBuffer);
    descriptorSetGenerator.setAccelerationStructure(1, *m_topLevelAccelerationStructure);
    descriptorSetGenerator.setBuffers(2, m_buffers);

    if (!m_descriptorSet)
        m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout->getResource()));
    m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

uint32_t RayTracedWorldManager::addStorageBuffer(const Wolf::ResourceNonOwner<Wolf::Buffer>& buffer)
{
    m_buffers.push_back(buffer);

    return m_buffers.size() - 1;
}
