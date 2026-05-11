#pragma once

#include "AssetInterface.h"
#include "BottomLevelAccelerationStructure.h"
#include "MeshAssetEditor.h"
#include "MeshFormatter.h"

class AssetMesh : public AssetInterface
{
public:
	AssetMesh(AssetManager* assetManager, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateAssetInUICallback,
		const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface, ExternalSceneLoader::MeshData& meshData, uint32_t defaultMaterialId, AssetId parentAssetId,
		const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
		const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU);
	AssetMesh(const AssetMesh&) = delete;
	~AssetMesh() override;

	void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
	void forceReload(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);
	void requestThumbnailReload();

	void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const override { outEditors.push_back(m_meshAssetEditor.createNonOwnerResource<ComponentInterface>()); }

	bool isLoaded() const override;
	Wolf::ResourceNonOwner<Wolf::Mesh> getMesh() { return m_mesh.createNonOwnerResource(); }
	bool isCentered() const { return m_isCentered; }
	const std::vector<MeshFormatter::LODInfo>& getDefaultLODInfo() const { return m_defaultLODsInfo; }
	const std::vector<MeshFormatter::LODInfo>& getSloppyLODInfo() const { return m_sloppyLODsInfo; }
	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> getDefaultSimplifiedMeshes() const;
	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> getSloppySimplifiedMeshes() const;
	bool isAnimated() const { return static_cast<bool>(m_animationData);}
	Wolf::ResourceNonOwner<AnimationData> getAnimationData() const { return m_animationData.createNonOwnerResource(); }
	std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>>& getPhysicsShapes() { return m_physicsShapes; }
	Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(uint32_t lod, uint32_t lodType);
	uint32_t getMaterialIdx() const { return m_materialIdx; }

private:
	AssetManager* m_assetManager = nullptr;
	Wolf::ResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;
	std::vector<Vertex3D> m_staticVertices;
	std::vector<SkeletonVertex> m_skeletonVertices;
	std::vector<uint32_t> m_indices;
	uint32_t m_materialIdx;

	void loadMeshFormatter(Wolf::ResourceUniqueOwner<MeshFormatter>& meshFormatter);
	void loadModel();
	struct LoadedMeshData
	{
		std::vector<Vertex3D> m_staticVertices;
		std::vector<SkeletonVertex> m_skeletonVertices;
		std::vector<uint32_t> m_indices;

		Wolf::ResourceUniqueOwner<AnimationData> m_animationData;
	};
	void loadModelFromData(LoadedMeshData& outLoadedMeshData);

	void computeThumbnailGenerationViewMatrix(const Wolf::AABB& aabb);
	void generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);
	void requestThumbnailReload(const glm::mat4& viewMatrix);
	void ensureBLASIsLoaded(uint32_t lod, uint32_t lodType);
	void buildBLAS(uint32_t lod, uint32_t lodType, const std::string& filename);

	Wolf::ResourceUniqueOwner<MeshAssetEditor> m_meshAssetEditor;

	bool m_meshLoadingRequested = false;
	bool m_thumbnailGenerationRequested = false;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_mesh;

	bool m_isCentered = false;

	std::vector<MeshFormatter::LODInfo> m_defaultLODsInfo;
	std::vector<MeshFormatter::LODInfo> m_sloppyLODsInfo;
	Wolf::DynamicResourceUniqueOwnerArray<Wolf::Mesh, 16> m_defaultSimplifiedMeshes;
	Wolf::DynamicResourceUniqueOwnerArray<Wolf::Mesh, 16> m_sloppySimplifiedMeshes;

	Wolf::ResourceUniqueOwner<AnimationData> m_animationData;

	std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>> m_physicsShapes;

	std::vector<std::vector<Wolf::ResourceUniqueOwner<Wolf::BottomLevelAccelerationStructure>>> m_bottomLevelAccelerationStructures;
	struct BlasId
	{
		uint32_t m_lodType;
		uint32_t m_lod;
	};
	BlasId m_loadedBLAS;
	std::vector<std::pair<BlasId, uint32_t /* frame index */>> m_BLASesToDestroy;

	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_meshToKeepInMemory;

	glm::mat4 m_thumbnailGenerationViewMatrix;
};