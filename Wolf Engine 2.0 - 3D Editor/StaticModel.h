#pragma once

#include <ModelLoader.h>
#include <PipelineSet.h>

#include "EditorModelInterface.h"
#include "ResourceManager.h"

class StaticModel : public EditorModelInterface
{
public:
	StaticModel(const glm::mat4& transform, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ResourceManager>& resourceManager);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	static inline std::string ID = "staticModel";
	std::string getId() const override { return ID; }

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	Wolf::AABB getAABB() const override;

	std::string getTypeString() override { return "staticMesh"; }

private:
	void requestModelLoading();
	EditorParamString m_loadingPathParam = EditorParamString("Mesh", "Model", "Loading", [this] { requestModelLoading(); }, EditorParamString::ParamStringType::FILE_OBJ);

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, StaticModel>> m_defaultPipelineSet;

	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<ResourceManager> m_resourceManager;
	ResourceManager::ResourceId m_meshResourceId = ResourceManager::NO_RESOURCE;

};

