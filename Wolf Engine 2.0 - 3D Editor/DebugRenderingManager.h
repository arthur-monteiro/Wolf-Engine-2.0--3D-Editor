#pragma once

#include <glm/glm.hpp>

#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Mesh.h>
#include <PipelineSet.h>
#include <ResourceNonOwner.h>

#include "Entity.h"

namespace Wolf
{
	class RenderMeshList;
}

class DebugRenderingManager
{
public:
	DebugRenderingManager();

	void updateGraphic();
	void addMeshesToRenderList(Wolf::RenderMeshList& renderMeshList) const;

	void setSelectedEntity(const Wolf::ResourceNonOwner<Entity>& entity);

private:
	// Lines
	struct LineVertex
	{
		glm::vec3 pos;
		glm::vec3 color;

		static void getBindingDescription(VkVertexInputBindingDescription& bindingDescription, uint32_t binding)
		{
			bindingDescription.binding = binding;
			bindingDescription.stride = sizeof(LineVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		static void getAttributeDescriptions(std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, uint32_t binding)
		{
			const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
			attributeDescriptions.resize(attributeDescriptionCountBefore + 2);

			attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 0].location = 0;
			attributeDescriptions[attributeDescriptionCountBefore + 0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(LineVertex, pos);

			attributeDescriptions[attributeDescriptionCountBefore + 1].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 1].location = 1;
			attributeDescriptions[attributeDescriptionCountBefore + 1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 1].offset = offsetof(LineVertex, color);
		}

		bool operator==(const LineVertex& other) const
		{
			return pos == other.pos && color == other.color;
		}
	};

	struct LinesUBData
	{
		glm::mat4 transform;
	};

	std::unique_ptr<Wolf::PipelineSet> m_linesPipelineSet;
	std::unique_ptr<Wolf::DescriptorSetLayoutGenerator> m_linesDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_linesDescriptorSetLayout;

	struct PerGroupOfLines
	{
		std::unique_ptr<Wolf::DescriptorSet> linesDescriptorSet;
		std::unique_ptr<Wolf::Buffer> linesUniformBuffer;
	};
	std::vector<PerGroupOfLines> m_perGroupOfLinesInfoArray;

	// AABBs
	std::unique_ptr<Wolf::Buffer> m_AABBVertexBuffer;
	std::unique_ptr<Wolf::Buffer> m_AABBIndexBuffer;
	std::unique_ptr<Wolf::Mesh> m_AABBMesh;

	std::vector<Wolf::ResourceNonOwner<Entity>> m_entitiesForAABBDraw;
};