#pragma once

#include <AABB.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Mesh.h>

#include "ModelInterface.h"

class BuildingModel : public ModelInterface
{
public:
	struct InstanceData
	{
		glm::vec3 offset;
		glm::vec3 newNormal;

		static void getBindingDescription(VkVertexInputBindingDescription& bindingDescription, uint32_t binding)
		{
			bindingDescription.binding = binding;
			bindingDescription.stride = sizeof(InstanceData);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		}

		static void getAttributeDescriptions(std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, uint32_t binding, uint32_t startLocation)
		{
			const uint32_t attributeDescriptionCountBefore = attributeDescriptions.size();
			attributeDescriptions.resize(attributeDescriptionCountBefore + 2);

			attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 0].location = startLocation;
			attributeDescriptions[attributeDescriptionCountBefore + 0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(InstanceData, offset);

			attributeDescriptions[attributeDescriptionCountBefore + 1].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 1].location = startLocation + 1;
			attributeDescriptions[attributeDescriptionCountBefore + 1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 1].offset = offsetof(InstanceData, newNormal);
		}
	};

	BuildingModel(const glm::mat4& transform, const std::string& filepath, uint32_t materialIdOffset);
	
	void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const override;

	const Wolf::AABB& getAABB() const override { return Wolf::AABB(); }
	const std::string& getLoadingPath() const override { return  m_filepath; }

	ModelType getType() override { return ModelType::BUILDING; }
	float getBuildingHeight() const { return m_fullSize.y; }
	float getBuildingSizeX() const { return m_fullSize.x; }
	float getBuildingSizeZ() const { return m_fullSize.z; }
	float getWindowSizeSize() const { return m_windowSideSizeInMeter;  }
	uint32_t getFloorCount() const { return m_floorCount; }
	void getImages(std::vector<Wolf::Image*>& images);

	void setBuildingSizeX(float value);
	void setBuildingSizeZ(float value);
	void setWindowSideSize(float value);
	void setFloorCount(uint32_t value);

	void loadWindowMesh(const std::string& filename, const std::string& materialFolder, uint32_t materialIdOffset);

	void save() const;

private:
	void setDefaultWindowMesh(uint32_t materialIdOffset);

	float computeFloorSize() const;
	float computeWindowCountOnSide(const glm::vec3& sideDir) const;
	void rebuildRenderingInfos();

	std::string m_filepath;

	/* User data */
	glm::vec3 m_fullSize;

	struct MeshWithMaterials
	{
		std::string loadingPath;
		std::unique_ptr<Wolf::Mesh> mesh;
		std::vector<std::unique_ptr<Wolf::Image>> images;
		glm::vec2 meshSizeInMeter;
		glm::vec2 center;
	};

	// Windows
	float m_windowSideSizeInMeter;
	float m_windowHeightInMeter;
	MeshWithMaterials m_windowMesh;

	// Floors
	uint32_t m_floorCount;

	/* Rendering info */
	std::unique_ptr<Wolf::DescriptorSet> m_windowsDescriptorSet;

	std::unique_ptr<Wolf::Buffer> m_windowInstanceBuffer;
	uint32_t m_windowInstanceCount;
	std::unique_ptr<Wolf::Buffer> m_windowsInfoUniformBuffer;
};

namespace Building
{
	struct MeshInfo
	{
		glm::vec2 scale;
		glm::vec2 offset;
	};

	extern Wolf::DescriptorSetLayoutGenerator g_descriptorSetLayoutGenerator;
	extern VkDescriptorSetLayout g_descriptorSetLayout;
}