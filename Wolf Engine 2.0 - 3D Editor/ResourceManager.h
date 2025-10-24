#pragma once

#include <BottomLevelAccelerationStructure.h>
#include <ModelLoader.h>

#include "ComponentInterface.h"
#include "MeshResourceEditor.h"
#include "RenderingPipelineInterface.h"
#include "ThumbnailsGenerationPass.h"

class Entity;

class ResourceManager
{
public:
	using ResourceId = uint32_t;
	static constexpr ResourceId NO_RESOURCE = -1;

	ResourceManager(const std::function<void(const std::string&, const std::string&, ResourceId)>& addResourceToUICallback, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback, 
		const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback, 
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback);

	void updateBeforeFrame();
	void save();
	void clear();

	Wolf::ResourceNonOwner<Entity> computeResourceEditor(ResourceId resourceId);

	static bool isMesh(ResourceId resourceId);
	static bool isImage(ResourceId resourceId);

	[[nodiscard]] ResourceId addModel(const std::string& loadingPath);
	bool isModelLoaded(ResourceId modelResourceId) const;
	Wolf::ModelData* getModelData(ResourceId modelResourceId) const;
	Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS(ResourceId modelResourceId);
	uint32_t getFirstMaterialIdx(ResourceId modelResourceId) const;
	uint32_t getFirstTextureSetIdx(ResourceId modelResourceId) const;
	void subscribeToResource(ResourceId resourceId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const;

	[[nodiscard]] ResourceId addImage(const std::string& loadingPath, bool loadMips, bool isSRGB, bool isHDR, bool keepDataOnCPU);
	bool isImageLoaded(ResourceId imageResourceId) const;
	Wolf::ResourceNonOwner<Wolf::Image> getImage(ResourceId imageResourceId) const;
	const uint8_t* getImageData(ResourceId imageResourceId) const;
	void deleteImageData(ResourceId imageResourceId) const;

private:
	static std::string computeModelFullIdentifier(const std::string& loadingPath);
	static std::string computeIconPath(const std::string& loadingPath, uint32_t thumbnailsLockedCount);
	static bool formatIconPath(const std::string& inLoadingPath, std::string& outIconPath);
	MeshResourceEditor* findMeshResourceEditorInResourceEditionToSave(ResourceId resourceId);
	void addCurrentResourceEditionToSave();
	void onResourceEditionChanged(Notifier::Flags flags);
	void requestThumbnailReload(ResourceId resourceId, const glm::mat4& viewMatrix);

	std::function<void(const std::string&, const std::string&, ResourceId)> m_addResourceToUICallback;
	std::function<void(const std::string&, const std::string&, ResourceId)> m_updateResourceInUICallback;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	std::function<void(const std::string&)> m_isolateMeshCallback;
	std::function<void(glm::mat4&)> m_removeIsolationAndGetViewMatrixCallback;

	class ResourceInterface : public Notifier
	{
	public:
		ResourceInterface(std::string loadingPath, ResourceId resourceId, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback);
		ResourceInterface(const ResourceInterface&) = delete;
		virtual ~ResourceInterface() = default;

		virtual void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) = 0;

		virtual bool isLoaded() const = 0;
		std::string getLoadingPath() const { return m_loadingPath; }
		std::string computeName();

		void onChanged();

	protected:
		std::string m_loadingPath;
		ResourceId m_resourceId = NO_RESOURCE;
		std::function<void(const std::string&, const std::string&, ResourceId)> m_updateResourceInUICallback;

		// Because the thumbnail file is locked by the UI, when we want to update it we need to create a new file with a new name
		// This counter indicates the name of the next file: (icon name)_(m_thumbnailCountToMaintain)_.(extension)
		uint32_t m_thumbnailCountToMaintain = 0;
	};

	class Mesh : public ResourceInterface
	{
	public:
		Mesh(const std::string& loadingPath, bool needThumbnailsGeneration, ResourceId resourceId, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback);
		Mesh(const Mesh&) = delete;
		~Mesh() override = default;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
		void forceReload(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);
		void requestThumbnailReload();

		void setThumbnailGenerationViewMatrix(const glm::mat4& viewMatrix) { m_thumbnailGenerationViewMatrix = viewMatrix;}

		bool isLoaded() const override;
		Wolf::ModelData* getModelData() { return &m_modelData; }
		Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure> getBLAS() { return m_bottomLevelAccelerationStructure.createNonOwnerResource(); }
		uint32_t getFirstMaterialIdx() const { return m_firstMaterialIdx; }
		uint32_t getFirstTextureSetIdx() const { return m_firstTextureSetIdx; }

	private:
		void loadModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager);
		void generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& m_thumbnailsGenerationPass);
		void buildBLAS();

		bool m_modelLoadingRequested = false;
		bool m_thumbnailGenerationRequested = false;
		Wolf::ModelData m_modelData;
		Wolf::ResourceUniqueOwner<Wolf::BottomLevelAccelerationStructure> m_bottomLevelAccelerationStructure;

		uint32_t m_firstTextureSetIdx = 0;
		uint32_t m_firstMaterialIdx = 0;

		Wolf::ResourceUniqueOwner<Wolf::Mesh> m_meshToKeepInMemory;

		glm::mat4 m_thumbnailGenerationViewMatrix;
	};

	class Image : public ResourceInterface
	{
	public:
		Image(const std::string& loadingPath, bool needThumbnailsGeneration, ResourceId resourceId, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback,
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

	static constexpr uint32_t MESH_RESOURCE_IDX_OFFSET = 0;
	static constexpr uint32_t MAX_MESH_RESOURCE_COUNT = 1000;
	std::vector<Wolf::ResourceUniqueOwner<Mesh>> m_meshes;

	static constexpr uint32_t IMAGE_RESOURCE_IDX_OFFSET = MESH_RESOURCE_IDX_OFFSET + MAX_MESH_RESOURCE_COUNT;
	static constexpr uint32_t MAX_IMAGE_RESOURCE_COUNT = 1000;
	std::vector<Wolf::ResourceUniqueOwner<Image>> m_images;

	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<ThumbnailsGenerationPass> m_thumbnailsGenerationPass;

	// Edition
	struct ResourceEditedParamsToSave
	{
		Wolf::ResourceUniqueOwner<MeshResourceEditor> meshResourceEditor;
		ResourceId resourceId;
	};
	static constexpr uint32_t MAX_EDITED_RESOURCES = 16;
	ResourceEditedParamsToSave m_resourceEditedParamsToSave[MAX_EDITED_RESOURCES];
	uint32_t m_resourceEditedParamsToSaveCount = 0;

	Wolf::ResourceUniqueOwner<Entity> m_transientEditionEntity;
	Wolf::NullableResourceNonOwner<MeshResourceEditor> m_meshResourceEditor;
	ResourceId m_currentResourceInEdition = NO_RESOURCE;
	uint32_t m_currentResourceNeedRebuildFlags = 0;
};

