#pragma once

#include <BottomLevelAccelerationStructure.h>
#include <ModelLoader.h>

#include "ComponentInterface.h"
#include "MeshAssetEditor.h"
#include "RenderingPipelineInterface.h"
#include "ThumbnailsGenerationPass.h"

class Entity;

class AssetManager
{
public:
	using AssetId = uint32_t;
	static constexpr AssetId NO_ASSET = -1;

	AssetManager(const std::function<void(const std::string&, const std::string&, AssetId)>& addAssetToUICallback, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
		const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback, 
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback);

	void updateBeforeFrame();
	void save();
	void clear();

	Wolf::ResourceNonOwner<Entity> computeResourceEditor(AssetId assetId);

	static bool isMesh(AssetId assetId);
	static bool isImage(AssetId assetId);

	[[nodiscard]] AssetId addModel(const std::string& loadingPath);
	bool isModelLoaded(AssetId modelResourceId) const;
	Wolf::ModelData* getModelData(AssetId modelResourceId) const;
	Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(AssetId modelResourceId, uint32_t lod, uint32_t lodType);
	uint32_t getFirstMaterialIdx(AssetId modelResourceId) const;
	uint32_t getFirstTextureSetIdx(AssetId modelResourceId) const;
	void subscribeToResource(AssetId resourceId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const;

	[[nodiscard]] AssetId addImage(const std::string& loadingPath, bool loadMips, bool isSRGB, bool isHDR, bool keepDataOnCPU);
	bool isImageLoaded(AssetId imageResourceId) const;
	Wolf::ResourceNonOwner<Wolf::Image> getImage(AssetId imageResourceId) const;
	const uint8_t* getImageData(AssetId imageResourceId) const;
	void deleteImageData(AssetId imageResourceId) const;

private:
	static std::string computeModelFullIdentifier(const std::string& loadingPath);
	static std::string computeIconPath(const std::string& loadingPath, uint32_t thumbnailsLockedCount);
	static bool formatIconPath(const std::string& inLoadingPath, std::string& outIconPath);
	MeshAssetEditor* findMeshResourceEditorInResourceEditionToSave(AssetId resourceId);
	void addCurrentResourceEditionToSave();
	void onResourceEditionChanged(Notifier::Flags flags);
	void requestThumbnailReload(AssetId resourceId, const glm::mat4& viewMatrix);

	std::function<void(const std::string&, const std::string&, AssetId)> m_addResourceToUICallback;
	std::function<void(const std::string&, const std::string&, AssetId)> m_updateResourceInUICallback;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	std::function<void(const std::string&)> m_isolateMeshCallback;
	std::function<void(glm::mat4&)> m_removeIsolationAndGetViewMatrixCallback;
	Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;

	class ResourceInterface : public Notifier
	{
	public:
		ResourceInterface(std::string loadingPath, AssetId resourceId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback);
		ResourceInterface(const ResourceInterface&) = delete;
		virtual ~ResourceInterface() = default;

		virtual void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) = 0;

		virtual bool isLoaded() const = 0;
		std::string getLoadingPath() const { return m_loadingPath; }
		std::string computeName();

		void onChanged();

	protected:
		std::string m_loadingPath;
		AssetId m_resourceId = NO_ASSET;
		std::function<void(const std::string&, const std::string&, AssetId)> m_updateAssetInUICallback;

		// Because the thumbnail file is locked by the UI, when we want to update it we need to create a new file with a new name
		// This counter indicates the name of the next file: (icon name)_(m_thumbnailCountToMaintain)_.(extension)
		uint32_t m_thumbnailCountToMaintain = 0;
	};

	class Mesh : public ResourceInterface
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
		Wolf::ModelData* getModelData() { return &m_modelData; }
		Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(uint32_t lod, uint32_t lodType);
		uint32_t getFirstMaterialIdx() const { return m_firstMaterialIdx; }
		uint32_t getFirstTextureSetIdx() const { return m_firstTextureSetIdx; }

	private:
		void loadModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager);
		void generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& m_thumbnailsGenerationPass);
		void buildBLAS(uint32_t lod, uint32_t lodType);

		bool m_modelLoadingRequested = false;
		bool m_thumbnailGenerationRequested = false;
		Wolf::ModelData m_modelData;
		std::vector<std::vector<Wolf::ResourceUniqueOwner<Wolf::BottomLevelAccelerationStructure>>> m_bottomLevelAccelerationStructures;

		uint32_t m_firstTextureSetIdx = 0;
		uint32_t m_firstMaterialIdx = 0;

		Wolf::ResourceUniqueOwner<Wolf::Mesh> m_meshToKeepInMemory;

		glm::mat4 m_thumbnailGenerationViewMatrix;
	};

	class Image : public ResourceInterface
	{
	public:
		Image(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId resourceId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
			bool loadMips, bool isSRGB, bool isHDR, bool keepDataOnCPU);
		Image(const Image&) = delete;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
		bool isLoaded() const override;

		Wolf::ResourceNonOwner<Wolf::Image> getImage() { return m_image.createNonOwnerResource(); }
		const uint8_t* getFirstMipData() const;
		void deleteImageData();

	private:
		void loadImage();

		bool m_imageLoadingRequested = false;
		bool m_thumbnailGenerationRequested = false;
		Wolf::ResourceUniqueOwner<Wolf::Image> m_image;

		bool m_loadMips;
		bool m_isSRGB;
		bool m_isHDR;

		enum class DataOnCPUStatus { NEVER_KEPT, NOT_LOADED_YET, AVAILABLE, DELETED } m_dataOnCPUStatus;
		std::vector<uint8_t> m_firstMipData;
	};

	static constexpr uint32_t MESH_ASSET_IDX_OFFSET = 0;
	static constexpr uint32_t MAX_ASSET_RESOURCE_COUNT = 1000;
	std::vector<Wolf::ResourceUniqueOwner<Mesh>> m_meshes;

	static constexpr uint32_t IMAGE_ASSET_IDX_OFFSET = MESH_ASSET_IDX_OFFSET + MAX_ASSET_RESOURCE_COUNT;
	static constexpr uint32_t MAX_IMAGE_ASSET_COUNT = 1000;
	std::vector<Wolf::ResourceUniqueOwner<Image>> m_images;

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
};

