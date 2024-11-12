#include "DrawManager.h"

#include <DynamicResourceUniqueOwnerArray.h>

#include "UpdateGPUBuffersPass.h"

DrawManager::DrawManager(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
	: m_renderMeshList(renderMeshList), m_updateGPUBuffersPass(renderingPipeline->getUpdateGPUBuffersPass())
{

}

void DrawManager::addMeshesToDraw(const std::vector<DrawMeshInfo>& meshesToRender, Entity* entity)
{
	if (m_infoByEntities.contains(entity))
	{
		const std::vector<InfoByEntity>& infoForEntity = m_infoByEntities[entity];
		bool hasSameMeshes = meshesToRender.size() == infoForEntity.size();
		for (uint32_t i = 0; hasSameMeshes && i < meshesToRender.size(); ++i)
		{
			if (!m_instancedMeshesRegistered[infoForEntity[i].instancedMeshRegisteredIdx]->isSame(meshesToRender[i].meshToRender))
				hasSameMeshes = false;
		}

		if (hasSameMeshes) // Only update instance data
		{
			for (uint32_t i = 0; i < meshesToRender.size(); ++i)
			{
				const DrawMeshInfo& meshToDraw = meshesToRender[i];
				Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_instancedMeshesRegistered[infoForEntity[i].instancedMeshRegisteredIdx];

				UpdateGPUBuffersPass::Request request(&meshToDraw.instanceData, sizeof(InstanceData), instancedMeshRegistered->getInstanceBuffer().createNonOwnerResource(), 
					infoForEntity[i].instanceIdx * sizeof(InstanceData));
				m_updateGPUBuffersPass->addRequestBeforeFrame(request);
			}

			return;
		}
		else
		{
			for (const InfoByEntity& info : infoForEntity)
			{
				m_renderMeshList->removeInstance(m_instancedMeshesRegistered[info.instancedMeshRegisteredIdx]->getInstancedMeshIdx(), info.instanceIdx);
			}
			m_infoByEntities[entity].clear();
		}
	}

	for (const DrawMeshInfo& meshToDraw : meshesToRender)
	{
		const Wolf::RenderMeshList::MeshToRender& meshToRender = meshToDraw.meshToRender;

		bool instancedMeshFound = false;
		DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_instancedMeshesRegistered, instancedMeshRegistered,
			{
				if (instancedMeshRegistered->isSame(meshToRender))
				{
					instancedMeshFound = true;
					uint32_t instanceIdx = instancedMeshRegistered->addInstance();
					addInstanceDataToBuffer(*instancedMeshRegistered, instanceIdx, meshToDraw.instanceData);

					m_infoByEntities[entity].push_back({ i, instanceIdx });

					break;
				}
			})

		if (!instancedMeshFound)
		{
			m_instancedMeshesRegistered.emplace_back(new InstancedMeshRegistered(meshToRender));
			addInstanceDataToBuffer(*m_instancedMeshesRegistered.back(), 0, meshToDraw.instanceData);

			m_infoByEntities[entity].push_back({ static_cast<uint32_t>(m_instancedMeshesRegistered.size()) - 1, 0 });
		}
	}
}

void DrawManager::addInstanceDataToBuffer(InstancedMeshRegistered& instancedMeshRegistered, uint32_t instanceIdx, const InstanceData& instanceData)
{
	Wolf::RenderMeshList::InstancedMesh instancedMesh = { instancedMeshRegistered.getMeshToRender(), instancedMeshRegistered.getInstanceBuffer().createNonOwnerResource(), sizeof(InstanceData) };
	if (instanceIdx == 0)
	{
		instancedMeshRegistered.setInstancedMeshIdx(m_renderMeshList->registerInstancedMesh(instancedMesh, MAX_INSTANCE_PER_MESH, instanceIdx));
	}
	else
		m_renderMeshList->addInstance(instancedMeshRegistered.getInstancedMeshIdx(), instanceIdx);

	UpdateGPUBuffersPass::Request request(&instanceData, sizeof(InstanceData), instancedMeshRegistered.getInstanceBuffer().createNonOwnerResource(), instanceIdx * sizeof(InstanceData));
	m_updateGPUBuffersPass->addRequestBeforeFrame(request);
}

DrawManager::InstancedMeshRegistered::InstancedMeshRegistered(Wolf::RenderMeshList::MeshToRender meshToRender) : m_meshToRender(std::move(meshToRender))
{
	uint32_t bufferSize = MAX_INSTANCE_PER_MESH * sizeof(InstanceData);
	m_instanceBuffer.reset(Wolf::Buffer::createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
}

bool DrawManager::InstancedMeshRegistered::isSame(const Wolf::RenderMeshList::MeshToRender& otherMeshToRender) const
{
	bool hasSameDescriptorSets = m_meshToRender.perPipelineDescriptorSets.size() == otherMeshToRender.perPipelineDescriptorSets.size();
	for (uint32_t pipelineIdx = 0; hasSameDescriptorSets && pipelineIdx < m_meshToRender.perPipelineDescriptorSets.size(); ++pipelineIdx)
	{
		hasSameDescriptorSets = m_meshToRender.perPipelineDescriptorSets[pipelineIdx].size() == otherMeshToRender.perPipelineDescriptorSets[pipelineIdx].size();

		for (uint32_t descriptorSetIdx = 0; hasSameDescriptorSets && descriptorSetIdx < m_meshToRender.perPipelineDescriptorSets[pipelineIdx].size(); ++descriptorSetIdx)
		{
			if (m_meshToRender.perPipelineDescriptorSets[pipelineIdx][descriptorSetIdx].getDescriptorSet() != otherMeshToRender.perPipelineDescriptorSets[pipelineIdx][descriptorSetIdx].getDescriptorSet())
				hasSameDescriptorSets = false;
		}
	}

	return hasSameDescriptorSets && m_meshToRender.mesh == otherMeshToRender.mesh && m_meshToRender.pipelineSet == otherMeshToRender.pipelineSet;
}
