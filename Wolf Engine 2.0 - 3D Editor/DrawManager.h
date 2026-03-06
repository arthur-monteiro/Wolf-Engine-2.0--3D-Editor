#pragma once

#include <DynamicResourceUniqueOwnerArray.h>
#include <DefaultMeshRenderer.h>
#include <InstanceMeshRenderer.h>
#include <ResourceUniqueOwner.h>

#include "RenderingPipelineInterface.h"

class Entity;

struct InstanceData
{
	glm::mat4 transform;
	uint32_t firstMaterialIdx;
	uint32_t entityIdx;
};

class DrawManager
{
public:
	DrawManager(const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface);

	struct DrawMeshInfo
	{
		Wolf::InstanceMeshRenderer::MeshToRender meshToRender;
		InstanceData instanceData;
	};
	void addMeshesToDraw(const std::vector<DrawMeshInfo>& meshesToRender, Entity* entity);
	void removeMeshesForEntity(Entity* entity);
	void clear();

	void isolateEntity(Entity* entity);
	void removeIsolation();

	void activateCameras(const Wolf::CameraList& cameraList) const;

private:
	Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer> m_instanceMeshRenderer;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
	Wolf::ResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;

	static constexpr uint32_t MAX_INSTANCE_PER_MESH = 2048;
	class InstancedMeshRegistered
	{
	public:
		InstancedMeshRegistered(const Wolf::InstanceMeshRenderer::MeshToRender& meshToRender, const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer);

		uint32_t getMeshIdx() const { return m_meshIdx; }

		bool isSame(const Wolf::InstanceMeshRenderer::MeshToRender& otherMeshToRender) const;

	private:
		Wolf::ResourceNonOwner<Wolf::MeshInterface> m_mesh;
		uint32_t m_meshIdx = -1;
	};
	Wolf::DynamicResourceUniqueOwnerArray<InstancedMeshRegistered, 64> m_meshesRegistered;

	struct InfoByEntity
	{
		uint32_t m_instancedMeshRegisteredIdx;
		uint32_t m_instanceIdx;
	};
	std::map<Entity*, std::vector<InfoByEntity>> m_infoByEntities;

	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_customInstanceCullingBuffer;
	Entity* m_isolatedEntity = nullptr;

	std::mutex m_meshMutex;
};

