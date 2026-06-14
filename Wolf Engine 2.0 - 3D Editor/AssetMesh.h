#pragma once

#include "AssetInterface.h"
#include "BottomLevelAccelerationStructure.h"
#include "MeshAssetEditor.h"
#include "MeshFormatter.h"

class AssetMesh : public AssetInterface
{
public:
	AssetMesh(AssetManager* assetManager, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(AssetId)>& onAssetUpdateCallback,
		const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface, ExternalSceneLoader::MeshData& meshData, uint32_t defaultMaterialIdx, AssetId parentAssetId,
		const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
		const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU);
	AssetMesh(const AssetMesh&) = delete;
	~AssetMesh() override;

	void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
	void forceReload(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);
	void requestThumbnailReload();

	void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const override { outEditors.push_back(m_meshAssetEditor.createNonOwnerResource<ComponentInterface>()); }

	bool isLoaded() const override;
	void loadLOD(uint32_t lodIdx, uint32_t lodType);

	uint32_t getDefaultSimplifiedLODCount() const { return m_defaultSimplifiedMeshes.size(); }
	uint32_t getSloppySimplifiedLODCount() const { return m_sloppySimplifiedMeshes.size(); }
	struct LOD
	{
		Wolf::NullableResourceNonOwner<Wolf::Mesh> m_mesh;
		uint32_t m_indexCount;

		std::vector<Wolf::InstanceMeshRenderer::MeshToRender::LOD::Cluster> m_clusters;
	};
	LOD getLOD(uint32_t lod, uint32_t lodType, bool ignoreDelayForLODInConstruction = false);

	bool isCentered() const { return m_isCentered; }
	bool isAnimated() const { return static_cast<bool>(m_animationData);}
	Wolf::ResourceNonOwner<AnimationData> getAnimationData() const { return m_animationData.createNonOwnerResource(); }
	std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>>& getPhysicsShapes() { return m_physicsShapes; }
	Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(uint32_t lod, uint32_t lodType);
	AssetId getDefaultMaterialAssetId() const { return m_materialAssetId; }
	Wolf::AABB getBoundingBox() const { return m_boundingBox; }
	Wolf::BoundingSphere getBoundingSphere() const { return m_boundingSphere; }

	[[nodiscard]] MeshFormatter* computeMeshFormatter();

private:
	AssetManager* m_assetManager = nullptr;
	Wolf::ResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;
	Wolf::ResourceNonOwner<Wolf::GPUDataTransfersManagerInterface> m_pushDataToGPUManager;
	std::vector<Vertex3D> m_staticVertices;
	std::vector<SkeletonVertex> m_skeletonVertices;
	std::vector<uint32_t> m_indices;
	std::string m_positionsCacheFilename;
	std::string m_indicesCacheFilename;
	AssetId m_materialAssetId = NO_ASSET;

	void loadMeshFormatter(Wolf::ResourceUniqueOwner<MeshFormatter>& meshFormatter, MeshFormatter::ReadSpecificLODInfo readSpecificLODInfo = MeshFormatter::ReadSpecificLODInfo());
	void loadMesh();
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

	Wolf::AABB m_boundingBox;
	Wolf::BoundingSphere m_boundingSphere;
	bool m_isCentered = false;

	struct InternalLOD
	{
		Wolf::ResourceUniqueOwner<Wolf::Mesh> m_mesh;
		uint32_t m_indexCount;

		std::vector<Wolf::InstanceMeshRenderer::MeshToRender::LOD::Cluster> m_clusterRanges;
	};
	InternalLOD m_mesh;
	Wolf::DynamicResourceUniqueOwnerArray<InternalLOD, 16> m_defaultSimplifiedMeshes;
	Wolf::DynamicResourceUniqueOwnerArray<InternalLOD, 16> m_sloppySimplifiedMeshes;

	struct LODInConstruction
	{
		uint32_t m_lod;
		uint32_t m_lodType;
		uint32_t m_buildFrameIdx;
	};
	std::vector<LODInConstruction> m_lodsInConstruction;

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