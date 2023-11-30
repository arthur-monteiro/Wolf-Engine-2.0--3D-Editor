#pragma once

#include <ModelLoader.h>
#include <PipelineSet.h>

#include "EditorModelInterface.h"

class ObjectModel : public EditorModelInterface
{
public:
	ObjectModel(const glm::mat4& transform, const std::string& filename, const std::string& mtlFolder, bool loadMaterials, uint32_t materialIdOffset);
	~ObjectModel() override = default;
	
	void addMeshesToRenderList(Wolf::RenderMeshList& renderMeshList) const override;

	const Wolf::AABB& getAABB() const override;
	const std::string& getLoadingPath() const override { return  m_filename; }

	ModelType getType() override { return ModelType::STATIC_MESH; }
	void getImages(std::vector<Wolf::Image*>& outputImages) const { m_modelData.getImages(outputImages); }

private:
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, ObjectModel>> m_defaultPipelineSet;
	
	std::string m_filename;
	Wolf::ModelData m_modelData;
};

