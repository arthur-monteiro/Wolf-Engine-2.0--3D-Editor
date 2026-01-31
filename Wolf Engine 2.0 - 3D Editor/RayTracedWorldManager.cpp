#include "RayTracedWorldManager.h"

#include <fstream>

#include <Buffer.h>
#include <DescriptorSetGenerator.h>
#include <DescriptorSetLayoutGenerator.h>
#include <GPUDataTransfersManager.h>

#include "ProfilerCommon.h"

RayTracedWorldManager::RayTracedWorldManager(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU) : m_editorPushDataToGPU(editorPushDataToGPU)
{
    m_instanceBuffer.reset(Wolf::Buffer::createBuffer(MAX_INSTANCES * sizeof(InstanceData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    m_descriptorSetLayoutGenerator.reset();
    m_descriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::RAYGEN | Wolf::ShaderStageFlagBits::CLOSEST_HIT, 0); // instance buffer
    m_descriptorSetLayoutGenerator.addAccelerationStructure(Wolf::ShaderStageFlagBits::RAYGEN | Wolf::ShaderStageFlagBits::CLOSEST_HIT, 1); // TLAS
    m_descriptorSetLayoutGenerator.addStorageBuffers(Wolf::ShaderStageFlagBits::RAYGEN | Wolf::ShaderStageFlagBits::CLOSEST_HIT, 2,
        2 * MAX_INSTANCES, Wolf::DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT); // Vertex and indices buffers

    m_descriptorSetLayout.reset(new Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, RayTracedWorldManager>([this](Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& descriptorSetLayout)
    {
        descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts(),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT));
    }));

    m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout->getResource()));
}

void RayTracedWorldManager::requestBuild(const RayTracedWorldInfo& info)
{
    PROFILE_FUNCTION

    if (info.m_instances.size() > MAX_INSTANCES)
    {
        Wolf::Debug::sendCriticalError("Too much BLAS instances, instance buffer will be too small");
    }
    m_buffers.clear();

    populateInstanceBuffer(info);
    requestBuildTLAS(info);
}

void RayTracedWorldManager::build(const Wolf::CommandBuffer& commandBuffer)
{
    m_topLevelAccelerationStructure->build(&commandBuffer, m_blasInstances);
    m_needsRebuildTLAS = false;
}

void RayTracedWorldManager::recordTLASBuildBarriers(const Wolf::CommandBuffer& commandBuffer)
{
    m_topLevelAccelerationStructure->recordBuildBarriers(&commandBuffer);
}

void RayTracedWorldManager::addRayGenShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot)
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

void RayTracedWorldManager::requestBuildTLAS(const RayTracedWorldInfo& info)
{
    PROFILE_FUNCTION

    m_blasInstances.resize(info.m_instances.size());
    for (size_t i = 0; i < m_blasInstances.size(); i++)
    {
        m_blasInstances[i].bottomLevelAS = info.m_instances[i].m_bottomLevelAccelerationStructure.operator->();
        m_blasInstances[i].transform = info.m_instances[i].m_transform;
        m_blasInstances[i].instanceID = i;
        m_blasInstances[i].hitGroupIndex = 0;
    }

    if (!m_topLevelAccelerationStructure || m_topLevelAccelerationStructure->getInstanceCount() != m_blasInstances.size())
    {
        m_topLevelAccelerationStructure.reset(Wolf::TopLevelAccelerationStructure::createTopLevelAccelerationStructure(m_blasInstances.size()));
    }

    uint64_t bufferListHash = computeBufferListHash();
    if (bufferListHash != m_buffersListHash)
    {
        // TODO: descriptor set may be in use, updating it may cause a crash
        createDescriptorSet(); // we need to update the descriptor set because buffers may have changed
        m_buffersListHash = bufferListHash;
    }

    m_needsRebuildTLAS = true;
}

void RayTracedWorldManager::populateInstanceBuffer(const RayTracedWorldInfo& info)
{
    PROFILE_FUNCTION

    m_instanceData.resize(info.m_instances.size());
    for (size_t i = 0; i < m_instanceData.size(); i++)
    {
        InstanceData& data = m_instanceData[i];
        const RayTracedWorldInfo::InstanceInfo& instanceInfo = info.m_instances[i];

        data.firstMaterialIdx = instanceInfo.m_firstMaterialIdx;
        data.vertexBufferBindlessOffset = addStorageBuffer(instanceInfo.m_mesh->getVertexBuffer());
        if (!instanceInfo.m_overrideIndexBuffer)
        {
            data.indexBufferBindlessOffset = addStorageBuffer(instanceInfo.m_mesh->getIndexBuffer());
        }
        else
        {
            data.indexBufferBindlessOffset = addStorageBuffer(instanceInfo.m_overrideIndexBuffer);
        }
    }

    m_editorPushDataToGPU->pushDataToGPUBuffer(m_instanceData.data(), m_instanceData.size() * sizeof(InstanceData), m_instanceBuffer.createNonOwnerResource(), 0);
}

void RayTracedWorldManager::createDescriptorSet()
{
    PROFILE_FUNCTION

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

uint64_t RayTracedWorldManager::computeBufferListHash() const
{
    static_assert(sizeof(Wolf::Buffer*) == sizeof(uint64_t));
    std::vector<uint64_t> bufferPtrs(m_buffers.size());
    for (const Wolf::ResourceNonOwner<Wolf::Buffer>& buffer : m_buffers)
    {
        bufferPtrs.push_back(reinterpret_cast<uint64_t>(buffer.operator->()));
    }

    return xxh64::hash(reinterpret_cast<const char*>(bufferPtrs.data()), bufferPtrs.size() * sizeof(uint64_t), 0);
}

