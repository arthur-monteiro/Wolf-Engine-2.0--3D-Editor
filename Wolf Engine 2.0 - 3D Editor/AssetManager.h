#pragma once

#include <BottomLevelAccelerationStructure.h>

#include "AssetId.h"
#include "ComponentInterface.h"
#include "EditorConfiguration.h"
#include "MeshAssetEditor.h"
#include "ModelLoader.h"
#include "RenderingPipelineInterface.h"
#include "ThumbnailsGenerationPass.h"

class Entity;

class AssetManager
{
public:
	AssetManager(const std::function<void(const std::string&, const std::string&, AssetId)>& addAssetToUICallback, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
		const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback, 
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
		const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU);

	void updateBeforeFrame();
	void save();
	void clear();

	Wolf::ResourceNonOwner<Entity> computeResourceEditor(AssetId assetId);

	static bool isMesh(AssetId assetId);
	static bool isImage(AssetId assetId);
	static bool isCombinedImage(AssetId assetId);

	[[nodiscard]] AssetId addModel(const std::string& loadingPath);
	bool isModelLoaded(AssetId modelResourceId) const;
	ModelData* getModelData(AssetId modelResourceId) const;
	Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(AssetId modelResourceId, uint32_t lod, uint32_t lodType);
	uint32_t getFirstMaterialIdx(AssetId modelResourceId) const;
	uint32_t getFirstTextureSetIdx(AssetId modelResourceId) const;
	void subscribeToMesh(AssetId assetId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const;

	[[nodiscard]] AssetId addImage(const std::string& loadingPath, bool loadMips, Wolf::Format format, bool keepDataOnCPU, bool canBeVirtualized, bool forceImmediateLoading = false);
	bool isImageLoaded(AssetId imageResourceId) const;
	Wolf::ResourceNonOwner<Wolf::Image> getImage(AssetId imageAssetId) const;
	const uint8_t* getImageData(AssetId imageAssetId) const;
	void deleteImageData(AssetId imageAssetId) const;
	std::string getImageSlicesFolder(AssetId imageAssetId) const;

	[[nodiscard]] AssetId addCombinedImage(const std::string& loadingPathR, const std::string& loadingPathG, const std::string& loadingPathB, const std::string& loadingPathA, bool loadMips, Wolf::Format format,
		bool keepDataOnCPU, bool canBeVirtualized, bool forceImmediateLoading = false);
	Wolf::ResourceNonOwner<Wolf::Image> getCombinedImage(AssetId combinedImageAssetId) const;
	std::string getCombinedImageSlicesFolder(AssetId combinedImageAssetId) const;

private:
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
	Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;
	Wolf::ResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;

	class AssetInterface : public Notifier
	{
	public:
		AssetInterface(std::string loadingPath, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback);
		AssetInterface(const AssetInterface&) = delete;
		virtual ~AssetInterface() = default;

		virtual void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) = 0;

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
		Mesh(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId resourceId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback);
		Mesh(const Mesh&) = delete;
		~Mesh() override = default;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
		void forceReload(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);
		void requestThumbnailReload();

		void setThumbnailGenerationViewMatrix(const glm::mat4& viewMatrix) { m_thumbnailGenerationViewMatrix = viewMatrix;}

		bool isLoaded() const override;
		ModelData* getModelData() { return &m_modelData; }
		Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(uint32_t lod, uint32_t lodType);
		uint32_t getFirstMaterialIdx() const { return m_firstMaterialIdx; }
		uint32_t getFirstTextureSetIdx() const { return m_firstTextureSetIdx; }

	private:
		void loadModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager);
		void generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& m_thumbnailsGenerationPass);
		void buildBLAS(uint32_t lod, uint32_t lodType);

		bool m_modelLoadingRequested = false;
		bool m_thumbnailGenerationRequested = false;
		ModelData m_modelData;
		std::vector<std::vector<Wolf::ResourceUniqueOwner<Wolf::BottomLevelAccelerationStructure>>> m_bottomLevelAccelerationStructures;

		uint32_t m_firstTextureSetIdx = 0;
		uint32_t m_firstMaterialIdx = 0;

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

	protected:
		ImageInterface(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, bool needThumbnailsGeneration, bool loadMips, Wolf::Format format, bool canBeVirtualized)
			: m_editorPushDataToGPU(editorPushDataToGPU), m_thumbnailGenerationRequested(!g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration), m_loadMips(loadMips), m_format(format),
			  m_canBeVirtualized(canBeVirtualized)
		{}

		bool generateThumbnail(const std::string& fullFilePath, const std::string& iconPath);

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

	private:
		void loadImage();

		std::array<std::string, 4> m_loadingPaths;
	};

	static constexpr uint32_t MESH_ASSET_IDX_OFFSET = 0;
	static constexpr uint32_t MAX_ASSET_RESOURCE_COUNT = 1000;
	std::vector<Wolf::ResourceUniqueOwner<Mesh>> m_meshes;

	static constexpr uint32_t IMAGE_ASSET_IDX_OFFSET = MESH_ASSET_IDX_OFFSET + MAX_ASSET_RESOURCE_COUNT;
	static constexpr uint32_t MAX_IMAGE_ASSET_COUNT = 1000;
	std::vector<Wolf::ResourceUniqueOwner<Image>> m_images;

	static constexpr uint32_t COMBINED_IMAGE_ASSET_IDX_OFFSET = IMAGE_ASSET_IDX_OFFSET + MAX_IMAGE_ASSET_COUNT;
	static constexpr uint32_t MAX_COMBINED_IMAGE_ASSET_COUNT = 1000;
	std::vector<Wolf::ResourceUniqueOwner<CombinedImage>> m_combinedImages;

	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<ThumbnailsGenerationPass> m_thumbnailsGenerationPass;

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

