#pragma once

#include <DynamicResourceUniqueOwnerArray.h>
#include <DefaultMeshRenderer.h>
#include <InstanceMeshRenderer.h>
#include <ResourceUniqueOwner.h>

#include "AssetId.h"
#include "RenderingPipelineInterface.h"

class AssetManager;
class Entity;

struct InstanceData
{
	glm::mat4 m_transform;
	uint32_t m_materialIdx;
	uint32_t m_entityIdx;
};

class DrawManager
{
public:
	DrawManager(const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
		const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface, const Wolf::ResourceNonOwner<AssetManager>& assetManager);

	struct DrawMeshInfo
	{
		AssetId m_meshAssetId;
		Wolf::NullableResourceNonOwner<Wolf::MeshInterface> m_mesh; // if not an asset

		Wolf::ResourceNonOwner<const Wolf::PipelineSet> m_pipelineSet;
		std::array<std::vector<Wolf::DescriptorSetBindInfo>, Wolf::PipelineSet::MAX_PIPELINE_COUNT> m_perPipelineDescriptorSets;
		InstanceData m_instanceData;
	};
	void addMeshesToDraw(const std::vector<DrawMeshInfo>& meshesToRender, Entity* entity);
	void removeMeshesForEntity(Entity* entity);
	void clear();

	void isolateEntity(Entity* entity);
	void removeIsolation();

	void activateCameras(const Wolf::CameraList& cameraList) const;
	void updateStreaming();

private:
	Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer> m_instanceMeshRenderer;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
	Wolf::ResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;
	Wolf::ResourceNonOwner<AssetManager> m_assetManager;

	Wolf::InstanceMeshRenderer::MeshToRender computeMeshToRender(AssetId meshAssetId, const Wolf::ResourceNonOwner<const Wolf::PipelineSet>& pipelineSet,
		const std::array<std::vector<Wolf::DescriptorSetBindInfo>, Wolf::PipelineSet::MAX_PIPELINE_COUNT>& perPipelineDescriptorSets) const;

	static constexpr uint32_t MAX_INSTANCE_PER_MESH = 2048;
	class InstancedMeshRegistered
	{
	public:
		InstancedMeshRegistered(const Wolf::InstanceMeshRenderer::MeshToRender& meshToRender, AssetId meshAssetId, const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer);

		void requestLoadingForLOD(uint32_t lod, const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer, const Wolf::ResourceNonOwner<AssetManager>& assetManager);

		uint32_t getMeshIdx() const { return m_meshIdx; }

		bool isSame(const Wolf::InstanceMeshRenderer::MeshToRender& otherMeshToRender) const;

	private:
		AssetId m_meshAssetId = NO_ASSET;
		Wolf::NullableResourceNonOwner<Wolf::MeshInterface> m_lowLODMesh;
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

