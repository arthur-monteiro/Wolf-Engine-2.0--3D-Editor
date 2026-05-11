#pragma once

#include <BottomLevelAccelerationStructure.h>

#include "AssetCombinedImage.h"
#include "AssetExternalScene.h"
#include "AssetId.h"
#include "AssetImage.h"
#include "AssetMaterial.h"
#include "AssetMesh.h"
#include "AssetParticle.h"
#include "AssetTextureSet.h"
#include "ComponentInterface.h"
#include "EditorConfiguration.h"
#include "ExternalSceneLoader.h"
#include "MeshAssetEditor.h"
#include "RenderingPipelineInterface.h"
#include "ThumbnailsGenerationPass.h"

class TextureSetEditor;
class Entity;
class ImageFormatter;

class AssetManager
{
public:
	AssetManager(const std::function<void(const std::string&, const std::string&, const std::string&, AssetId, const std::string&)>& addAssetToUICallback, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
		const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback, 
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
		const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface);

	void updateBeforeFrame();
	void save(std::ofstream& outFile) const;
	void clear();

	void releaseRenderingPipeline();

	Wolf::ResourceNonOwner<Entity> computeAssetEditor(AssetId assetId);

	AssetId getAssetIdForPath(const std::string& path);

	static bool isMesh(AssetId assetId);
	static bool isImage(AssetId assetId);
	static bool isCombinedImage(AssetId assetId);
	static bool isScene(AssetId assetId);
	static bool isTextureSet(AssetId assetId);
	static bool isExternalScene(AssetId assetId);
	static bool isMaterial(AssetId assetId);
	static bool isParticle(AssetId assetId);

	bool isMeshLoaded(AssetId assetId) const;
	Wolf::ResourceNonOwner<Wolf::Mesh> getMesh(AssetId assetId) const;
	bool isMeshAnimated(AssetId assetId) const;
	const std::vector<MeshFormatter::LODInfo>& getMeshDefaultLODInfo(AssetId assetId) const;
	const std::vector<MeshFormatter::LODInfo>& getMeshSloppyLODInfo(AssetId assetId) const;
	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> getMeshDefaultSimplifiedMeshes(AssetId assetId) const;
	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> getMeshSloppySimplifiedMeshes(AssetId assetId) const;
	Wolf::ResourceNonOwner<AnimationData> getAnimationData(AssetId assetId) const;
	Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(AssetId assetId, uint32_t lod, uint32_t lodType);
	std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>>& getPhysicsShapes(AssetId modelAssetId) const;
	uint32_t getMaterialIdx(AssetId meshAssetId) const;
	std::string computeModelName(AssetId modelAssetId) const;
	void subscribeToMesh(AssetId assetId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const;

	AssetId addImage(const std::string& loadingPath, AssetId parentAssetId = NO_ASSET);
	bool isImageLoaded(AssetId assetId) const;
	void requestImageLoading(AssetId assetId, const AssetImageInterface::LoadingRequest& loadingRequest, bool requestImmediateLoading = false);
	Wolf::ResourceNonOwner<Wolf::Image> getImage(AssetId imageAssetId, Wolf::Format format) const;
	const uint8_t* getImageData(AssetId imageAssetId, uint32_t mipLevel, Wolf::Format format) const;
	void deleteImageData(AssetId imageAssetId) const;
	void releaseImage(AssetId imageAssetId) const;
	std::string getImageSlicesFolder(AssetId imageAssetId) const;
	std::string getImageLoadingPath(AssetId assetId) const;
	Wolf::ResourceNonOwner<ImageEditor> getImageEditor(AssetId assetId) const;

	AssetId addCombinedImage(const std::string& loadingPath, AssetId parentAssetId);
	void requestCombinedImageLoading(AssetId assetId, const AssetImageInterface::LoadingRequest& loadingRequest, bool requestImmediateLoading = false);
	Wolf::ResourceNonOwner<Wolf::Image> getCombinedImage(AssetId combinedImageAssetId, Wolf::Format format) const;
	std::string getCombinedImageSlicesFolder(AssetId combinedImageAssetId) const;
	Wolf::ResourceNonOwner<CombinedImageEditor> getCombinedImageEditor(AssetId assetId) const;

	AssetId addExternalScene(const std::string& loadingPath);
	bool isSceneLoaded(AssetId sceneAssetId) const;
	Wolf::AABB getSceneAABB(AssetId sceneAssetId) const;
	const std::vector<AssetId>& getSceneModelAssetIds(AssetId sceneAssetId) const;
	const std::vector<ExternalSceneLoader::InstanceData>& getSceneInstances(AssetId sceneAssetId) const;

	AssetId addTextureSet(const std::string& loadingPath, AssetId parentAssetId = NO_ASSET);
	Wolf::ResourceNonOwner<TextureSetEditor> getTextureSetEditor(AssetId assetId) const;

	AssetId addMaterial(const std::string& loadingPath, AssetId parentAssetId = NO_ASSET);
	bool isMaterialLoaded(AssetId assetId) const;
	Wolf::ResourceNonOwner<MaterialEditor> getMaterialEditor(AssetId assetId) const;

	AssetId addParticle(const std::string& loadingPath);
	Wolf::ResourceNonOwner<ParticleEditor> getParticleEditor(AssetId assetId) const;

private:
	friend class AssetMesh;
	friend class AssetImage;
	friend class AssetCombinedImage;
	friend class AssetExternalScene;
	friend class AssetTextureSet;

	[[nodiscard]] AssetId addMesh(ExternalSceneLoader::MeshData& meshData, const std::string& name, uint32_t materialIdx, AssetId parentAssetId);
	[[nodiscard]] AssetId addMeshInternal(const std::string& loadingPath, ExternalSceneLoader::MeshData& meshData, uint32_t defaultMaterialId, AssetId parentAssetId = -1);

	static std::string computeModelFullIdentifier(const std::string& loadingPath);
	static std::string computeIconPath(const std::string& loadingPath, uint32_t thumbnailsLockedCount);
	static bool formatIconPath(const std::string& inLoadingPath, std::string& outIconPath);
	void releaseAllEditorsFromTransientEntity();
	void onAssetEditionChanged(Notifier::Flags flags);
	static bool saveAsset(std::stringstream& outStringStream, Wolf::ResourceNonOwner<AssetInterface> assetInterface);

	std::function<void(const std::string&, const std::string&, const std::string&, AssetId, const std::string&)> m_addAssetToUICallback;
	std::function<void(const std::string&, const std::string&, AssetId)> m_updateResourceInUICallback;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	std::function<void(const std::string&)> m_isolateMeshCallback;
	std::function<void(glm::mat4&)> m_removeIsolationAndGetViewMatrixCallback;
	Wolf::NullableResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;
	Wolf::ResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;
	Wolf::ResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;

	static constexpr uint32_t MESH_ASSET_IDX_OFFSET = 0;
	static constexpr uint32_t MAX_ASSET_RESOURCE_COUNT = 1000;
	Wolf::DynamicResourceUniqueOwnerArray<AssetMesh, 16> m_meshes;

	static constexpr uint32_t IMAGE_ASSET_IDX_OFFSET = MESH_ASSET_IDX_OFFSET + MAX_ASSET_RESOURCE_COUNT;
	static constexpr uint32_t MAX_IMAGE_ASSET_COUNT = 1000;
	Wolf::DynamicResourceUniqueOwnerArray<AssetImage, 16> m_images;

	static constexpr uint32_t COMBINED_IMAGE_ASSET_IDX_OFFSET = IMAGE_ASSET_IDX_OFFSET + MAX_IMAGE_ASSET_COUNT;
	static constexpr uint32_t MAX_COMBINED_IMAGE_ASSET_COUNT = 1000;
	Wolf::DynamicResourceUniqueOwnerArray<AssetCombinedImage, 16>  m_combinedImages;

	static constexpr uint32_t EXTERNAL_SCENE_ASSET_IDX_OFFSET = COMBINED_IMAGE_ASSET_IDX_OFFSET + MAX_COMBINED_IMAGE_ASSET_COUNT;
	static constexpr uint32_t MAX_EXTERNAL_SCENE_ASSET_COUNT = 32;
	Wolf::DynamicResourceUniqueOwnerArray<AssetExternalScene, 2>  m_externalScenes;

	static constexpr uint32_t TEXTURE_SET_ASSET_IDX_OFFSET = EXTERNAL_SCENE_ASSET_IDX_OFFSET + MAX_EXTERNAL_SCENE_ASSET_COUNT;
	static constexpr uint32_t MAX_TEXTURE_SET_ASSET_COUNT = 1000;
	Wolf::DynamicResourceUniqueOwnerArray<AssetTextureSet, 16>  m_textureSets;

	static constexpr uint32_t MATERIAL_ASSET_IDX_OFFSET = TEXTURE_SET_ASSET_IDX_OFFSET + MAX_TEXTURE_SET_ASSET_COUNT;
	static constexpr uint32_t MAX_MATERIAL_ASSET_COUNT = 1000;
	Wolf::DynamicResourceUniqueOwnerArray<AssetMaterial, 16>  m_materials;

	static constexpr uint32_t PARTICLE_ASSET_IDX_OFFSET = MATERIAL_ASSET_IDX_OFFSET + MAX_MATERIAL_ASSET_COUNT;
	static constexpr uint32_t MAX_PARTICLE_ASSET_COUNT = 128;
	Wolf::DynamicResourceUniqueOwnerArray<AssetParticle, 16>  m_particles;

	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::NullableResourceNonOwner<ThumbnailsGenerationPass> m_thumbnailsGenerationPass;

	Wolf::ResourceUniqueOwner<Entity> m_transientEditionEntity;
	AssetId m_currentAssetInEdition = NO_ASSET;
	uint32_t m_currentAssetNeedRebuildFlags = 0;

	static AssetManager* ms_assetManager;
};

