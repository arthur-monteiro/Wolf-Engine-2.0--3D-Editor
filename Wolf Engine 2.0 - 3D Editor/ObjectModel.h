#pragma once

#include <ObjLoader.h>
#include <PipelineSet.h>

#include "ModelInterface.h"

class ObjectModel : public ModelInterface
{
public:
	ObjectModel(const glm::mat4& transform, bool buildAccelerationStructures, const std::string& filename, const std::string& mtlFolder, bool loadMaterials, uint32_t materialIdOffset, const Wolf::BindlessDescriptor& bindlessDescriptor);
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

