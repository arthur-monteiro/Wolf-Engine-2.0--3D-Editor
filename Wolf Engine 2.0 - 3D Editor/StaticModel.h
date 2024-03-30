#pragma once

#include <ModelLoader.h>
#include <PipelineSet.h>

#include "EditorModelInterface.h"

class StaticModel : public EditorModelInterface
{
public:
	StaticModel(const glm::mat4& transform, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	static inline std::string ID = "staticModel";
	std::string getId() const override { return ID; }

	void updateBeforeFrame() override;
	void getMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& outList) override;
	void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	Wolf::AABB getAABB() const override;

	std::string getTypeString() override { return "staticMesh"; }

private:
	void loadModel();

	bool m_modelLoadingRequested = false;
	void requestModelLoading();
	EditorParamString m_loadingPathParam = EditorParamString("Mesh", "Model", "Loading", [this] { requestModelLoading(); }, EditorParamString::ParamStringType::FILE_OBJ);

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, StaticModel>> m_defaultPipelineSet;
	
	Wolf::ModelData m_modelData;
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
};

