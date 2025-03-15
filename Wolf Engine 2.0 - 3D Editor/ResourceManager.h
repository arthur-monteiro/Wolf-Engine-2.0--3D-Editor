#pragma once

#include <ModelLoader.h>

#include "ComponentInterface.h"
#include "MeshResourceEditor.h"
#include "PipelineSet.h"
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
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

	void updateBeforeFrame();
	void save();
	void clear();

	Wolf::ResourceNonOwner<Entity> computeResourceEditor(ResourceId resourceId);

	ResourceId addModel(const std::string& loadingPath);
	bool isModelLoaded(ResourceId modelResourceId) const;
	Wolf::ModelData* getModelData(ResourceId modelResourceId) const;
	uint32_t getFirstMaterialIdx(ResourceId modelResourceId) const;
	uint32_t getFirstTextureSetIdx(ResourceId modelResourceId) const;
	void subscribeToResource(ResourceId resourceId, const void* instance, const std::function<void()>& callback) const;

private:
	static std::string computeModelFullIdentifier(const std::string& loadingPath);
	static std::string computeIconPath(const std::string& loadingPath);
	MeshResourceEditor* findMeshResourceEditorInResourceEditionToSave(ResourceId resourceId);
	void addCurrentResourceEditionToSave();
	void onResourceEditionChanged(ResourceId resourceId, MeshResourceEditor* meshResourceEditor);

	std::function<void(const std::string&, const std::string&, ResourceId)> m_addResourceToUICallback;
	std::function<void(const std::string&, const std::string&, ResourceId)> m_updateResourceInUICallback;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;

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
	};

	class Mesh : public ResourceInterface
	{
	public:
		Mesh(const std::string& loadingPath, bool needThumbnailsGeneration, ResourceId resourceId, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback);
		Mesh(const Mesh&) = delete;
		~Mesh() override = default;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;

		bool isLoaded() const override;
		Wolf::ModelData* getModelData() { return &m_modelData; }
		uint32_t getFirstMaterialIdx() const { return m_firstMaterialIdx; }
		uint32_t getFirstTextureSetIdx() const { return m_firstTextureSetIdx; }

	private:
		void loadModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& m_thumbnailsGenerationPass);

		bool m_modelLoadingRequested = false;
		bool m_thumbnailGenerationRequested = false;
		Wolf::ModelData m_modelData;

		uint32_t m_firstTextureSetIdx = 0;
		uint32_t m_firstMaterialIdx = 0;
	};

	static constexpr uint32_t MESH_RESOURCE_IDX_OFFSET = 0;
	std::vector<Wolf::ResourceUniqueOwner<Mesh>> m_meshes;

	static constexpr uint32_t IMAGE_RESOURCE_IDX_OFFSET = 1000;

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
	ResourceId m_currentResourceInEdition = NO_RESOURCE;
};

