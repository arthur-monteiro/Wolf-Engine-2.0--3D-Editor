#pragma once

#include <DescriptorSet.h>
#include <ObjLoader.h>
#include <Buffer.h>

#include "ModelInterface.h"

class ObjectModel : public ModelInterface
{
public:
	ObjectModel(const glm::mat4& transform, bool buildAccelerationStructures, const std::string& filename, const std::string& mtlFolder, bool loadMaterials, uint32_t materialIdOffset);

	void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const override;

	const Wolf::AABB& getAABB() const override;
	const std::string& getLoadingPath() const override { return  m_filename; }

	ModelType getType() override { return ModelType::STATIC_MESH; }
	void getImages(std::vector<Wolf::Image*>& outputImages) const { m_modelData.getImages(outputImages); }

private:
	std::string m_filename;

	Wolf::ModelData m_modelData;
};

