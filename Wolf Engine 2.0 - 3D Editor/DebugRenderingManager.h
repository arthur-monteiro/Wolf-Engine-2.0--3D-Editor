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

		static void getBindingDescription(VkVertexInputBindingDescription& bindingDescription, uint32_t binding)
		{
			bindingDescription.binding = binding;
			bindingDescription.stride = sizeof(DebugVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		static void getAttributeDescriptions(std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, uint32_t binding)
		{
			const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
			attributeDescriptions.resize(attributeDescriptionCountBefore + 1);

			attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 0].location = 0;
			attributeDescriptions[attributeDescriptionCountBefore + 0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(DebugVertex, pos);
		}

		bool operator==(const DebugVertex& other) const
		{
			return pos == other.pos;
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
	void addSphere(const glm::vec3& worldPos, float radius, const glm::vec3& color);
	void addWiredSphere(const glm::vec3& worldPos, float radius, const glm::vec3& color);
	void addRectangle(const glm::mat4& transform);

	void addMeshesToRenderList(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList);

private:
	Wolf::ResourceUniqueOwner<Wolf::PipelineSet> m_linesPipelineSet;
	std::unique_ptr<Wolf::DescriptorSetLayoutGenerator> m_linesDescriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_linesDescriptorSetLayout;

	struct PerGroupOfLines
	{
		Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> linesDescriptorSet;
		Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> linesUniformBuffer;
		Wolf::ResourceNonOwner<Wolf::Mesh> mesh;

		PerGroupOfLines(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh, 
			const Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& linesDescriptorSetLayout, 
			const std::unique_ptr<Wolf::DescriptorSetLayoutGenerator>& linesDescriptorSetLayoutGenerator);
	};

	// AABBs
	std::unique_ptr<Wolf::Buffer> m_AABBVertexBuffer;
	std::unique_ptr<Wolf::Buffer> m_AABBIndexBuffer;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_cubeLineMesh;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_cubeQuadMesh;

	uint32_t m_AABBInfoArrayCount = 0;
	std::vector<PerGroupOfLines> m_AABBInfoArray;

	// Custom lines
	std::vector<uint32_t> m_customLinesInfoArrayCount;
	std::vector<std::vector<PerGroupOfLines>> m_customLinesInfoArray;
	std::mutex m_customLinesMutex;

	// Spheres
	static constexpr uint32_t MAX_SPHERE_COUNT = 128;
	struct SpheresUBData
	{
		glm::vec4 worldPosAndRadius[MAX_SPHERE_COUNT];
		glm::vec4 colors[MAX_SPHERE_COUNT];
	};

	class PerSpherePipeline
	{
	public:
		void init(Wolf::PolygonMode polygonMode);
		void add(const glm::vec3& worldPos, float radius, const glm::vec3& color);
		void registerMeshes(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList, Wolf::ResourceUniqueOwner<Wolf::Mesh>& cubeQuadMesh);
		void clear();

	private:
		Wolf::ResourceUniqueOwner<Wolf::PipelineSet> m_pipelineSet;
		std::unique_ptr<Wolf::DescriptorSetLayoutGenerator> m_descriptorSetLayoutGenerator;
		Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
		Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
		SpheresUBData m_uniformData;
		uint32_t m_count = 0;
		Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;
	};
	PerSpherePipeline m_filledSphereData;
	PerSpherePipeline m_wiredSphereData;

	// Rectangles
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_rectangleMesh;
	Wolf::ResourceUniqueOwner<Wolf::PipelineSet> m_rectanglesPipelineSet;
	std::unique_ptr<Wolf::DescriptorSetLayoutGenerator> m_rectanglesDescriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_rectanglesDescriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_rectanglesDescriptorSet;
	static constexpr uint32_t MAX_RECTANGLES_COUNT = 128;
	struct RectanglesUBData
	{
		glm::mat4 transform[MAX_RECTANGLES_COUNT];
	};
	RectanglesUBData m_rectanglesData;
	uint32_t m_rectanglesCount = 0;
	Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_rectanglesUniformBuffer;

	std::vector<Wolf::ResourceUniqueOwner<Wolf::RenderMeshList::MeshToRender>> m_meshesToRender;
};