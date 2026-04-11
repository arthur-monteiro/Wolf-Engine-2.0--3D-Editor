#pragma once

#include <BottomLevelAccelerationStructure.h>

#include "AssetId.h"
#include "ComponentInterface.h"
#include "EditorConfiguration.h"
#include "ExternalSceneLoader.h"
#include "MeshAssetEditor.h"
#include "ModelLoader.h"
#include "RenderingPipelineInterface.h"
#include "ThumbnailsGenerationPass.h"

class Entity;
class ImageFormatter;

class AssetManager
{
public:
	AssetManager(const std::function<void(const std::string&, const std::string&, AssetId)>& addAssetToUICallback, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
		const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback, 
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
		const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface);

	void updateBeforeFrame();
	void save();
	void clear();

	void releaseRenderingPipeline();

	void dump(const std::string& dumpLocalFolder);

	Wolf::ResourceNonOwner<Entity> computeResourceEditor(AssetId assetId);

	static bool isMesh(AssetId assetId);
	static bool isImage(AssetId assetId);
	static bool isCombinedImage(AssetId assetId);
	static bool isScene(AssetId assetId);

	[[nodiscard]] AssetId addModel(const std::string& loadingPath);
	bool isModelLoaded(AssetId modelAssetId) const;
	Wolf::ResourceNonOwner<Wolf::Mesh> getModelMesh(AssetId modelAssetId) const;
	bool isModelCentered(AssetId modelAssetId) const;
	const std::vector<MeshFormatter::LODInfo>& getModelDefaultLODInfo(AssetId modelAssetId) const;
	const std::vector<MeshFormatter::LODInfo>& getModelSloppyLODInfo(AssetId modelAssetId) const;
	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> getModelDefaultSimplifiedMeshes(AssetId modelAssetId) const;
	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> getModelSloppySimplifiedMeshes(AssetId modelAssetId) const;
	Wolf::ResourceNonOwner<AnimationData> getAnimationData(AssetId modelAssetId) const;
	Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(AssetId modelAssetId, uint32_t lod, uint32_t lodType);
	std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>>& getPhysicsShapes(AssetId modelAssetId) const;
	uint32_t getFirstMaterialIdx(AssetId modelAssetId) const;
	uint32_t getFirstTextureSetIdx(AssetId modelAssetId) const;
	const std::vector<Wolf::MaterialsGPUManager::TextureSetInfo>& getModelTextureSetInfo(AssetId modelAssetId) const;
	std::string computeModelName(AssetId modelAssetId) const;
	void subscribeToMesh(AssetId assetId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const;

	[[nodiscard]] AssetId addImage(const std::string& loadingPath, bool loadMips, Wolf::Format format, bool keepDataOnCPU, bool canBeVirtualized, bool forceImmediateLoading = false);
	bool isImageLoaded(AssetId imageResourceId) const;
	Wolf::ResourceNonOwner<Wolf::Image> getImage(AssetId imageAssetId) const;
	const uint8_t* getImageData(AssetId imageAssetId) const;
	void deleteImageData(AssetId imageAssetId) const;
	void releaseImage(AssetId imageAssetId) const;
	std::string getImageSlicesFolder(AssetId imageAssetId) const;

	[[nodiscard]] AssetId addCombinedImage(const std::string& loadingPathR, const std::string& loadingPathG, const std::string& loadingPathB, const std::string& loadingPathA, bool loadMips, Wolf::Format format,
		bool keepDataOnCPU, bool canBeVirtualized, bool forceImmediateLoading = false);
	Wolf::ResourceNonOwner<Wolf::Image> getCombinedImage(AssetId combinedImageAssetId) const;
	std::string getCombinedImageSlicesFolder(AssetId combinedImageAssetId) const;

	[[nodiscard]] AssetId addExternalScene(const std::string& loadingPath);
	bool isSceneLoaded(AssetId sceneAssetId) const;
	Wolf::AABB getSceneAABB(AssetId sceneAssetId) const;
	uint32_t getSceneFirstMaterialIdx(AssetId sceneAssetId) const;
	const std::vector<AssetId>& getSceneModelAssetIds(AssetId sceneAssetId) const;
	const std::vector<ExternalSceneLoader::InstanceData>& getSceneInstances(AssetId sceneAssetId) const;

private:
	[[nodiscard]] AssetId addModel(const std::vector<Vertex3D>& staticVertices, const std::vector<uint32_t>& indices, const std::string& name, uint32_t materialIdx);
	[[nodiscard]] AssetId addModelInternal(const std::string& loadingPath, const std::vector<Vertex3D>& overrideStaticVertices = {}, const std::vector<uint32_t>& overrideIndices = {},
		uint32_t overrideMaterialIdx = -1);

	static std::string computeModelFullIdentifier(const std::string& loadingPath);
	static std::string computeIconPath(const std::string& loadingPath, uint32_t thumbnailsLockedCount);
	static bool formatIconPath(const std::string& inLoadingPath, std::string& outIconPath);
	MeshAssetEditor* findMeshResourceEditorInResourceEditionToSave(AssetId resourceId);
	void addCurrentResourceEditionToSave();
	void onResourceEditionChanged(Notifier::Flags flags);
	void requestThumbnailReload(AssetId resourceId, const glm::mat4& viewMatrix);
	static std::string extractFilenameFromPath(const std::string& fullPath);
	static std::string extractFolderFromFullPath(const std::string& fullPath);

	std::function<void(const std::string&, const std::string&, AssetId)> m_addResourceToUICallback;
	std::function<void(const std::string&, const std::string&, AssetId)> m_updateResourceInUICallback;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	std::function<void(const std::string&)> m_isolateMeshCallback;
	std::function<void(glm::mat4&)> m_removeIsolationAndGetViewMatrixCallback;
	Wolf::NullableResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;
	Wolf::ResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;
	Wolf::ResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;

	class AssetInterface : public Notifier
	{
	public:
		AssetInterface(std::string loadingPath, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback);
		AssetInterface(const AssetInterface&) = delete;
		virtual ~AssetInterface() = default;

		virtual void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) = 0;
		virtual void dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath);

		virtual bool isLoaded() const = 0;
		std::string getLoadingPath() const { return m_loadingPath; }
		std::string computeName();

		void onChanged();

	protected:
		std::string m_loadingPath;
		AssetId m_assetId = NO_ASSET;
		std::function<void(const std::string&, const std::string&, AssetId)> m_updateAssetInUICallback;

		// Because the thumbnail file is locked by the UI, when we want to update it we need to create a new file with a new name
		// This counter indicates the name of the next file: (icon name)_(m_thumbnailCountToMaintain)_.(extension)
		uint32_t m_thumbnailCountToMaintain = 0;
	};

	class Mesh : public AssetInterface
	{
	public:
		Mesh(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
			const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface, const std::vector<Vertex3D>& overrideStaticVertices,
			const std::vector<uint32_t>& overrideIndices, uint32_t overrideMaterialIdx);
		Mesh(const Mesh&) = delete;
		~Mesh() override = default;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
		void forceReload(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);
		void requestThumbnailReload();

		void dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath) override;

		void setThumbnailGenerationViewMatrix(const glm::mat4& viewMatrix) { m_thumbnailGenerationViewMatrix = viewMatrix;}

		bool isLoaded() const override;
		Wolf::ResourceNonOwner<Wolf::Mesh> getMesh() { return m_mesh.createNonOwnerResource(); }
		bool isCentered() const { return m_isCentered; }
		const std::vector<MeshFormatter::LODInfo>& getDefaultLODInfo() const { return m_defaultLODsInfo; }
		const std::vector<MeshFormatter::LODInfo>& getSloppyLODInfo() const { return m_sloppyLODsInfo; }
		std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> getDefaultSimplifiedMeshes() const;
		std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> getSloppySimplifiedMeshes() const;
		bool isAnimated() const { return static_cast<bool>(m_animationData);}
		Wolf::ResourceNonOwner<AnimationData> getAnimationData() { return m_animationData.createNonOwnerResource(); }
		std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>>& getPhysicsShapes() { return m_physicsShapes; }
		Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(uint32_t lod, uint32_t lodType);
		uint32_t getFirstMaterialIdx() const { return m_firstMaterialIdx; }
		uint32_t getFirstTextureSetIdx() const { return m_firstTextureSetIdx; }
		const std::vector<Wolf::MaterialsGPUManager::TextureSetInfo>& getTextureSetInfo() const { return m_textureSetInfo; }

	private:
		Wolf::ResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;
		std::vector<Vertex3D> m_overrideStaticVertices;
		std::vector<uint32_t> m_overrideIndices;
		uint32_t m_overrideMaterialIdx;

		void loadModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager);
		struct LoadedMeshData
		{
			std::vector<Vertex3D> m_staticVertices;
			std::vector<SkeletonVertex> m_skeletonVertices;
			std::vector<uint32_t> m_indices;

			std::vector<TextureSetLoader::TextureSetFileInfoGGX> m_textureSetsInfo;
			Wolf::ResourceUniqueOwner<AnimationData> m_animationData;
		};
		void loadModelFromFile(LoadedMeshData& outLoadedMeshData);
		void loadModelFromData(LoadedMeshData& outLoadedMeshData);

		void computeThumbnailGenerationViewMatrix(const Wolf::AABB& aabb);
		void generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);
		void ensureBLASIsLoaded(uint32_t lod, uint32_t lodType);
		void buildBLAS(uint32_t lod, uint32_t lodType, const std::string& filename);

		bool m_modelLoadingRequested = false;
		bool m_thumbnailGenerationRequested = false;
		Wolf::ResourceUniqueOwner<Wolf::Mesh> m_mesh;

		bool m_isCentered = false;

		std::vector<MeshFormatter::LODInfo> m_defaultLODsInfo;
		std::vector<MeshFormatter::LODInfo> m_sloppyLODsInfo;
		std::vector<Wolf::ResourceUniqueOwner<Wolf::Mesh>> m_defaultSimplifiedMeshes;
		std::vector<Wolf::ResourceUniqueOwner<Wolf::Mesh>> m_sloppySimplifiedMeshes;

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

		uint32_t m_firstTextureSetIdx = 0;
		uint32_t m_firstMaterialIdx = 0;
		std::vector<Wolf::MaterialsGPUManager::TextureSetInfo> m_textureSetInfo;

		Wolf::ResourceUniqueOwner<Wolf::Mesh> m_meshToKeepInMemory;

		glm::mat4 m_thumbnailGenerationViewMatrix;
	};

	class ImageInterface
	{
	public:
		ImageInterface() = delete;

		Wolf::ResourceNonOwner<Wolf::Image> getImage() { return m_image.createNonOwnerResource(); }
		std::string getSlicesFolder() { return m_slicesFolder; }

		void deleteImageData();
		void releaseImage();

	protected:
		ImageInterface(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, bool needThumbnailsGeneration, bool loadMips, Wolf::Format format, bool canBeVirtualized)
			: m_editorPushDataToGPU(editorPushDataToGPU), m_thumbnailGenerationRequested(!g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration), m_loadMips(loadMips), m_format(format),
			  m_canBeVirtualized(canBeVirtualized)
		{}

		bool generateThumbnail(const std::string& fullFilePath, const std::string& iconPath);
		void dump(const std::string& folderPath, AssetId assetId, ImageFormatter& imageFormatter) const;

		Wolf::ResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;

		bool m_imageLoadingRequested = false;
		bool m_thumbnailGenerationRequested = false;
		Wolf::ResourceUniqueOwner<Wolf::Image> m_image;
		std::string m_slicesFolder;

		bool m_loadMips;
		Wolf::Format m_format;
		bool m_canBeVirtualized;

		enum class DataOnCPUStatus { NEVER_KEPT, NOT_LOADED_YET, AVAILABLE, DELETED } m_dataOnCPUStatus;
		std::vector<uint8_t> m_firstMipData;
	};

	class Image : public AssetInterface, public ImageInterface
	{
	public:
		Image(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
			bool loadMips, Wolf::Format format, bool keepDataOnCPU, bool canBeVirtualized);
		Image(const Image&) = delete;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
		bool isLoaded() const override;

		void dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath) override;

		const uint8_t* getFirstMipData() const;

	private:
		void loadImage();
	};

	class CombinedImage : public AssetInterface, public ImageInterface
	{
	public:
		CombinedImage(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& combinedLoadingPath, const std::string& loadingPathR, const std::string& loadingPathG,
			const std::string& loadingPathB, const std::string& loadingPathA, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
			bool loadMips, Wolf::Format format, bool keepDataOnCPU, bool canBeVirtualized);

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
		bool isLoaded() const override;

		void dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath) override;

	private:
		void loadImage();

		std::array<std::string, 4> m_loadingPaths;
	};

	class ExternalScene : public AssetInterface
	{
	public:
		ExternalScene(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback);
		ExternalScene(const ExternalScene&) = delete;
		~ExternalScene() override = default;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
		void dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath) override;

		bool isLoaded() const override;

		const Wolf::AABB& getAABB();
		const std::vector<AssetId>& getModelAssetIds() const { return m_modelAssetIds; }
		uint32_t getFirstMaterialIdx() const { return m_firstMaterialIdx; }
		const std::vector<ExternalSceneLoader::InstanceData>& getInstances() const { return m_instances; }
		void setThumbnailGenerationViewMatrix(const glm::mat4& viewMatrix) { m_thumbnailGenerationViewMatrix = viewMatrix; }

	private:
		void loadScene(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager);
		void generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);

		bool m_loadingRequested = false;
		bool m_thumbnailGenerationRequested = false;

		glm::mat4 m_thumbnailGenerationViewMatrix;

		std::vector<AssetId> m_modelAssetIds;
		Wolf::AABB m_aabb;
		uint32_t m_firstMaterialIdx;

		std::vector<ExternalSceneLoader::InstanceData> m_instances;
	};

	static constexpr uint32_t MESH_ASSET_IDX_OFFSET = 0;
	static constexpr uint32_t MAX_ASSET_RESOURCE_COUNT = 1000;
	Wolf::DynamicResourceUniqueOwnerArray<Mesh, 16> m_meshes;

	static constexpr uint32_t IMAGE_ASSET_IDX_OFFSET = MESH_ASSET_IDX_OFFSET + MAX_ASSET_RESOURCE_COUNT;
	static constexpr uint32_t MAX_IMAGE_ASSET_COUNT = 1000;
	Wolf::DynamicResourceUniqueOwnerArray<Image, 16> m_images;

	static constexpr uint32_t COMBINED_IMAGE_ASSET_IDX_OFFSET = IMAGE_ASSET_IDX_OFFSET + MAX_IMAGE_ASSET_COUNT;
	static constexpr uint32_t MAX_COMBINED_IMAGE_ASSET_COUNT = 1000;
	Wolf::DynamicResourceUniqueOwnerArray<CombinedImage, 16>  m_combinedImages;

	static constexpr uint32_t EXTERNAL_SCENE_ASSET_IDX_OFFSET = COMBINED_IMAGE_ASSET_IDX_OFFSET + MAX_COMBINED_IMAGE_ASSET_COUNT;
	static constexpr uint32_t MAX_EXTERNAL_SCENE_ASSET_COUNT = 32;
	Wolf::DynamicResourceUniqueOwnerArray<ExternalScene, 2>  m_externalScenes;

	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::NullableResourceNonOwner<ThumbnailsGenerationPass> m_thumbnailsGenerationPass;

	// Edition
	struct ResourceEditedParamsToSave
	{
		Wolf::ResourceUniqueOwner<MeshAssetEditor> meshAssetEditor;
		AssetId assetId;
	};
	static constexpr uint32_t MAX_EDITED_ASSET = 16;
	ResourceEditedParamsToSave m_assetEditedParamsToSave[MAX_EDITED_ASSET];
	uint32_t m_assetEditedParamsToSaveCount = 0;

	Wolf::ResourceUniqueOwner<Entity> m_transientEditionEntity;
	Wolf::NullableResourceNonOwner<MeshAssetEditor> m_meshAssetEditor;
	AssetId m_currentAssetInEdition = NO_ASSET;
	uint32_t m_currentAssetNeedRebuildFlags = 0;

	static AssetManager* ms_assetManager;
};

