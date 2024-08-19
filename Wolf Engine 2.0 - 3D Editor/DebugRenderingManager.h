#pragma once

#include <glm/glm.hpp>

#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Mesh.h>
#include <PipelineSet.h>
#include <ResourceNonOwner.h>
#include <ResourceUniqueOwner.h>

#include "Entity.h"

namespace Wolf
{
	class RenderMeshList;
}

class DebugRenderingManager
{
public:
	// Lines
	struct DebugVertex
	{
		glm::vec3 pos;
		glm::vec3 color;

		static void getBindingDescription(VkVertexInputBindingDescription& bindingDescription, uint32_t binding)
		{
			bindingDescription.binding = binding;
			bindingDescription.stride = sizeof(DebugVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		static void getAttributeDescriptions(std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, uint32_t binding)
		{
			const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
			attributeDescriptions.resize(attributeDescriptionCountBefore + 2);

			attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 0].location = 0;
			attributeDescriptions[attributeDescriptionCountBefore + 0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(DebugVertex, pos);

			attributeDescriptions[attributeDescriptionCountBefore + 1].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 1].location = 1;
			attributeDescriptions[attributeDescriptionCountBefore + 1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 1].offset = offsetof(DebugVertex, color);
		}

		bool operator==(const DebugVertex& other) const
		{
			return pos == other.pos && color == other.color;
		}
	};

	DebugRenderingManager();

	void clearBeforeFrame();

	struct LinesUBData
	{
		glm::mat4 transform;
	};
	void addAABB(const Wolf::AABB& box);
	void addCustomGroupOfLines(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh, const LinesUBData& data);
	void addSphere(const glm::vec3& worldPos, float radius);

	void addMeshesToRenderList(Wolf::RenderMeshList& renderMeshList);

private:
	Wolf::ResourceUniqueOwner<Wolf::PipelineSet> m_linesPipelineSet;
	std::unique_ptr<Wolf::DescriptorSetLayoutGenerator> m_linesDescriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_linesDescriptorSetLayout;

	struct PerGroupOfLines
	{
		Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> linesDescriptorSet;
		Wolf::ResourceUniqueOwner<Wolf::Buffer> linesUniformBuffer;
		Wolf::ResourceNonOwner<Wolf::Mesh> mesh;

		PerGroupOfLines(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh, 
			const Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& linesDescriptorSetLayout, 
			const std::unique_ptr<Wolf::DescriptorSetLayoutGenerator>& linesDescriptorSetLayoutGenerator);
	};

	// AABBs
	std::unique_ptr<Wolf::Buffer> m_AABBVertexBuffer;
	std::unique_ptr<Wolf::Buffer> m_AABBIndexBuffer;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_cubeLineMesh;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_cubeLQuadMesh;

	uint32_t m_AABBInfoArrayCount = 0;
	std::vector<PerGroupOfLines> m_AABBInfoArray;

	// Custom lines
	uint32_t m_customLinesInfoArrayCount = 0;
	std::vector<PerGroupOfLines> m_customLinesInfoArray;

	// Spheres
	Wolf::ResourceUniqueOwner<Wolf::PipelineSet> m_spheresPipelineSet;
	std::unique_ptr<Wolf::DescriptorSetLayoutGenerator> m_spheresDescriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_spheresDescriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_spheresDescriptorSet;
	static constexpr uint32_t MAX_SPHERE_COUNT = 16;
	struct SpheresUBData
	{
		glm::vec4 worldPosAndRadius[MAX_SPHERE_COUNT];
	};
	SpheresUBData m_spheresData;
	uint32_t m_sphereCount = 0;
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_spheresUniformBuffer;
};