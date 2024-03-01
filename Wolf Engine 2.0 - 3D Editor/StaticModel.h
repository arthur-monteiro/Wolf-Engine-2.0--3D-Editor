#pragma once

#include <ModelLoader.h>
#include <PipelineSet.h>

#include "EditorModelInterface.h"

class StaticModel : public EditorModelInterface
{
public:
	StaticModel(const glm::mat4& transform, const Wolf::ResourceNonOwner<Wolf::BindlessDescriptor>& bindlessDescriptor);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	static inline std::string ID = "staticModel";
	std::string getId() const override { return ID; }

	void updateGraphic() override;
	void addMeshesToRenderList(Wolf::RenderMeshList& renderMeshList) const override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	Wolf::AABB getAABB() const override;

	std::string getTypeString() override { return "staticMesh"; }
	void getImages(std::vector<Wolf::Image*>& outputImages) const { m_modelData.getImages(outputImages); }

private:
	void loadModel();

	bool m_modelLoadingRequested = false;
	void requestModelLoading();
	EditorParamString m_loadingPathParam = EditorParamString("Mesh", "Model", "Loading", [this] { requestModelLoading(); }, true);

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, StaticModel>> m_defaultPipelineSet;
	
	Wolf::ModelData m_modelData;
	Wolf::ResourceNonOwner<Wolf::BindlessDescriptor> m_bindlessDescriptor;
};

