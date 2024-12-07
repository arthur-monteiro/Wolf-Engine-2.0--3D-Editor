#pragma once

#include <ModelLoader.h>

#include "PipelineSet.h"
#include "RenderingPipelineInterface.h"
#include "ThumbnailsGenerationPass.h"

class ResourceManager
{
public:
	ResourceManager(const std::function<void(const std::string&, const std::string&)>& addResourceToUICallback, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, 
		const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline);

	void updateBeforeFrame();
	void clear();

	using ResourceId = uint32_t;
	static constexpr ResourceId NO_RESOURCE = -1;

	ResourceId addModel(const std::string& loadingPath);
	bool isModelLoaded(ResourceId modelResourceId) const;
	Wolf::ModelData* getModelData(ResourceId modelResourceId) const;
	uint32_t getFirstMaterialIdx(ResourceId modelResourceId) const;
	uint32_t getFirstTextureSetIdx(ResourceId modelResourceId) const;

private:
	static std::string computeModelFullIdentifier(const std::string& loadingPath);
	static std::string computeIconPath(const std::string& loadingPath);

	std::function<void(const std::string&, const std::string&)> m_addResourceToUICallback;

	class ResourceInterface
	{
	public:
		ResourceInterface(std::string loadingPath);
		ResourceInterface(const ResourceInterface&) = delete;
		virtual ~ResourceInterface() = default;

		virtual void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) = 0;

		virtual bool isLoaded() const = 0;
		std::string getLoadingPath() const { return m_loadingPath; }
		std::string computeName();

	protected:
		std::string m_loadingPath;
	};

	class Mesh : public ResourceInterface
	{
	public:
		Mesh(const std::string& loadingPath, bool needThumbnailsGeneration);
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
};

