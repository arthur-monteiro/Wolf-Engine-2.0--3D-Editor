#include "DrawManager.h"

#include <DynamicResourceUniqueOwnerArray.h>

#include "CameraList.h"
#include "CommonLayouts.h"
#include "UpdateGPUBuffersPass.h"

DrawManager::DrawManager(const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface)
	: m_instanceMeshRenderer(instanceMeshRenderer), m_updateGPUBuffersPass(renderingPipeline->getUpdateGPUBuffersPass()), m_bufferPoolInterface(bufferPoolInterface)
{

}

void DrawManager::addMeshesToDraw(const std::vector<DrawMeshInfo>& meshesToRender, Entity* entity)
{
	m_meshMutex.lock();

	// If the entity already registered meshes, we remove them then re-add to update data
	if (m_infoByEntities.contains(entity))
	{
		const std::vector<InfoByEntity>& infoForEntity = m_infoByEntities[entity];
		for (const InfoByEntity& info : infoForEntity)
		{
			m_instanceMeshRenderer->removeInstance(info.m_instanceIdx);
		}
		m_infoByEntities[entity].clear();
	}

	for (const DrawMeshInfo& meshToDraw : meshesToRender)
	{
		const Wolf::InstanceMeshRenderer::MeshToRender& meshToRender = meshToDraw.meshToRender;

		bool meshFound = false;
		for (uint32_t i = 0; i < m_meshesRegistered.size(); ++i)
		{
			Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_meshesRegistered[i];

			if (instancedMeshRegistered->isSame(meshToRender))
			{
				meshFound = true;
				uint32_t instanceIdx = m_instanceMeshRenderer->addInstance(instancedMeshRegistered->getMeshIdx(), meshToDraw.instanceData.transform, meshToDraw.instanceData.firstMaterialIdx,
					meshToDraw.instanceData.entityIdx, meshToRender.m_pipelineSet, meshToRender.m_perPipelineDescriptorSets);

				m_infoByEntities[entity].push_back({ i, instanceIdx });

				break;
			}
		}

		if (!meshFound)
		{
			Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_meshesRegistered.emplace_back(new InstancedMeshRegistered(meshToRender, m_instanceMeshRenderer));

			uint32_t instanceIdx = m_instanceMeshRenderer->addInstance(instancedMeshRegistered->getMeshIdx(), meshToDraw.instanceData.transform, meshToDraw.instanceData.firstMaterialIdx,
					meshToDraw.instanceData.entityIdx, meshToRender.m_pipelineSet, meshToRender.m_perPipelineDescriptorSets);

			m_infoByEntities[entity].push_back({ static_cast<uint32_t>(m_meshesRegistered.size()) - 1, instanceIdx });
		}
	}

	m_meshMutex.unlock();
}

void DrawManager::removeMeshesForEntity(Entity* entity)
{
	m_meshMutex.lock();

	if (m_infoByEntities.contains(entity))
	{
		const std::vector<InfoByEntity>& infoForEntity = m_infoByEntities[entity];
		for (const InfoByEntity& info : infoForEntity)
		{
			m_instanceMeshRenderer->removeInstance(info.m_instanceIdx);
		}
		m_infoByEntities[entity].clear();
	}

	m_meshMutex.unlock();
}

void DrawManager::clear()
{
	m_meshMutex.lock();

	m_meshesRegistered.clear();
	m_infoByEntities.clear();

	m_meshMutex.unlock();
}

void DrawManager::isolateEntity(Entity* entity)
{
	if (!m_infoByEntities.contains(entity))
	{
		Wolf::Debug::sendError("Trying to isolate an enregistered entity");
		return;
	}

	const std::vector<InfoByEntity>& infoForEntity = m_infoByEntities[entity];
	if (infoForEntity.empty())
	{
		Wolf::Debug::sendError("Trying to isolate an entity where info is empty");
		return;
	}

	if (infoForEntity.size() != 1)
	{
		Wolf::Debug::sendWarning("Isolated an entity doesn't have exactly 1 instance, this is not currently supported. Only the first instance will be isolated");
	}

	std::vector<Wolf::InstanceMeshRenderer::OverrideInstance> instancesForEntity;
	for (const InfoByEntity& info : infoForEntity)
	{
		instancesForEntity.emplace_back(info.m_instanceIdx);
	}
	m_instanceMeshRenderer->overrideCullingInstances(instancesForEntity);

	m_isolatedEntity = entity;
}

void DrawManager::removeIsolation()
{
	m_instanceMeshRenderer->stopOverridingCullingInstances();

	removeMeshesForEntity(m_isolatedEntity);
	m_isolatedEntity = nullptr;
}

void DrawManager::activateCameras(const Wolf::CameraList& cameraList) const
{
	m_instanceMeshRenderer->activateCameraForThisFrame(CommonCameraIndices::CAMERA_IDX_MAIN, CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH);
	m_instanceMeshRenderer->activateCameraForThisFrame(CommonCameraIndices::CAMERA_IDX_MAIN, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
	m_instanceMeshRenderer->activateCameraForThisFrame(CommonCameraIndices::CAMERA_IDX_MAIN, CommonPipelineIndices::PIPELINE_IDX_OUTPUT_IDS); // TODO: only enable if picking this frame

	uint32_t cameraCount = cameraList.getCurrentCameras().size();

	for (uint32_t cameraPipelineIdx = CommonCameraIndices::CAMERA_IDX_SHADOW_CASCADE_0; cameraPipelineIdx <= CommonCameraIndices::CAMERA_IDX_SHADOW_CASCADE_3; cameraPipelineIdx++)
	{
		if (cameraPipelineIdx < cameraCount)
		{
			m_instanceMeshRenderer->activateCameraForThisFrame(cameraPipelineIdx, CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
		}
	}

	for (uint32_t cameraPipelineIdx = CommonCameraIndices::CAMERA_IDX_FIRST_CUSTOM_RENDER_PASS; cameraPipelineIdx <= CommonCameraIndices::CAMERA_IDX_LAST_CUSTOM_RENDER_PASS; cameraPipelineIdx++)
	{
		if (cameraPipelineIdx < cameraCount)
		{
			m_instanceMeshRenderer->activateCameraForThisFrame(cameraPipelineIdx, CommonPipelineIndices::PIPELINE_IDX_CUSTOM_RENDER);
		}
	}
}

DrawManager::InstancedMeshRegistered::InstancedMeshRegistered(const Wolf::InstanceMeshRenderer::MeshToRender& meshToRender, const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer)
	: m_mesh(meshToRender.m_lods[0].m_mesh)
{
	m_meshIdx = instanceMeshRenderer->registerMesh(meshToRender);
}

bool DrawManager::InstancedMeshRegistered::isSame(const Wolf::InstanceMeshRenderer::MeshToRender& otherMeshToRender) const
{
	return m_mesh == otherMeshToRender.m_lods[0].m_mesh;
}
