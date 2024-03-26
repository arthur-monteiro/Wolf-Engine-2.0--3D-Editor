#include "DebugRenderingManager.h"

#include <glm/gtc/matrix_transform.hpp>

#include <Debug.h>
#include <DescriptorSetGenerator.h>
#include <DescriptorSetLayoutGenerator.h>
#include <RenderMeshList.h>

#include "CommonDescriptorLayouts.h"

DebugRenderingManager::DebugRenderingManager()
{
	m_linesDescriptorSetLayoutGenerator.reset(new Wolf::DescriptorSetLayoutGenerator);
	m_linesDescriptorSetLayoutGenerator->addUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0); // transform

	m_linesDescriptorSetLayout.reset(new Wolf::DescriptorSetLayout(m_linesDescriptorSetLayoutGenerator->getDescriptorLayouts()));

	m_linesPipelineSet.reset(new Wolf::PipelineSet);

	Wolf::PipelineSet::PipelineInfo pipelineInfo;

	pipelineInfo.shaderInfos.resize(2);
	pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/debug/lines.vert";
	pipelineInfo.shaderInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/debug/lines.frag";
	pipelineInfo.shaderInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	// IA
	LineVertex::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

	pipelineInfo.vertexInputBindingDescriptions.resize(1);
	LineVertex::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

	pipelineInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

	// Resources
	pipelineInfo.descriptorSetLayouts = { { m_linesDescriptorSetLayout->getDescriptorSetLayout(), 1 } , { CommonDescriptorLayouts::g_commonDescriptorSetLayout, 2 } };
	pipelineInfo.cameraDescriptorSlot = 0;

	// Color Blend
	pipelineInfo.blendModes = {Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

	// Dynamic states
	pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

	const uint32_t pipelineIdx = m_linesPipelineSet->addPipeline(pipelineInfo);
	if (pipelineIdx != 0)
		Wolf::Debug::sendError("Unexpected pipeline idx");

	// AABB
	std::vector<LineVertex> AABBVertices =
	{
		// Top
		{ glm::vec3(-0.5, 0.5f, -0.5f) }, // back left
		{ glm::vec3(0.5f, 0.5f, -0.5f) }, // back right
		{ glm::vec3(-0.5f, 0.5f, 0.5f) }, // front left
		{ glm::vec3(0.5f, 0.5f, 0.5f) }, // front right

		// Bot
		{ glm::vec3(-0.5, -0.5f, -0.5f) }, // back left
		{ glm::vec3(0.5f, -0.5f, -0.5f) }, // back right
		{ glm::vec3(-0.5f, -0.5f, 0.5f) }, // front left
		{ glm::vec3(0.5f, -0.5f, 0.5f) } // front right
	};

	std::vector<uint32_t> AABBIndices =
	{
		// Top
		0, 1, // back line
		2, 3, // front line
		0, 2, // left line
		1, 3, // right line

		// Bot
		4, 5, // back line
		6, 7, // front line
		4, 6, // left line
		5, 7, // right line

		// Top to bot
		0, 4, // back left
		1, 5, // back right
		2, 6, // front left
		3, 7 // front right
	};

	m_AABBMesh.reset(new Wolf::Mesh(AABBVertices, AABBIndices));
}

void DebugRenderingManager::clearBeforeFrame()
{
	m_AABBInfoArrayCount = 0;
	for (uint32_t i = 0; i < m_customInfoArrayCount; ++i)
		m_customInfoArray[i].mesh = m_AABBMesh.createNonOwnerResource(); // stop using the custom mesh as it can be deleted
	m_customInfoArrayCount = 0;
}

void DebugRenderingManager::addAABB(const Wolf::AABB& box)
{
	if (m_AABBInfoArrayCount >= m_AABBInfoArray.size())
	{
		m_AABBInfoArray.emplace_back(m_AABBMesh.createNonOwnerResource(), m_linesDescriptorSetLayout, m_linesDescriptorSetLayoutGenerator);
	}

	const PerGroupOfLines& perGroupOfLinesInfo = m_AABBInfoArray[m_AABBInfoArrayCount];

	LinesUBData data{};
	data.transform = glm::translate(glm::mat4(glm::mat4(1.0f)), glm::vec3(box.getCenter()));
	data.transform = glm::scale(data.transform, glm::vec3(box.getSize()));
	perGroupOfLinesInfo.linesUniformBuffer->transferCPUMemory(&data, sizeof(data));

	m_AABBInfoArrayCount++;
}

void DebugRenderingManager::addCustomGroupOfLines(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh, const LinesUBData& data)
{
	if (m_customInfoArrayCount >= m_customInfoArray.size())
	{
		m_customInfoArray.emplace_back(mesh, m_linesDescriptorSetLayout, m_linesDescriptorSetLayoutGenerator);
	}
	m_customInfoArray[m_customInfoArrayCount].mesh = mesh;
	m_customInfoArray[m_customInfoArrayCount].linesUniformBuffer->transferCPUMemory(&data, sizeof(data));
	m_customInfoArrayCount++;
}

void DebugRenderingManager::addMeshesToRenderList(Wolf::RenderMeshList& renderMeshList)
{
	for (uint32_t i = 0; i < m_AABBInfoArrayCount; ++i)
	{
		PerGroupOfLines& AABBInfo = m_AABBInfoArray[i];

		Wolf::RenderMeshList::MeshToRenderInfo meshToRenderInfo(AABBInfo.mesh, m_linesPipelineSet.get());
		meshToRenderInfo.descriptorSets.push_back({ AABBInfo.linesDescriptorSet.createConstNonOwnerResource(), 1});

		renderMeshList.addMeshToRender(meshToRenderInfo);
	}

	for (uint32_t i = 0; i < m_customInfoArrayCount; ++i)
	{
		PerGroupOfLines& customInfo = m_customInfoArray[i];

		Wolf::RenderMeshList::MeshToRenderInfo meshToRenderInfo(customInfo.mesh, m_linesPipelineSet.get());
		meshToRenderInfo.descriptorSets.push_back({ customInfo.linesDescriptorSet.createConstNonOwnerResource(), 1 });

		renderMeshList.addMeshToRender(meshToRenderInfo);
	}
}

DebugRenderingManager::PerGroupOfLines::PerGroupOfLines(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh,
	const std::unique_ptr<Wolf::DescriptorSetLayout>& linesDescriptorSetLayout,
	const std::unique_ptr<Wolf::DescriptorSetLayoutGenerator>& linesDescriptorSetLayoutGenerator) : mesh(mesh)
{
	linesUniformBuffer.reset(new Wolf::Buffer(sizeof(LinesUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Wolf::UpdateRate::NEVER));
	linesDescriptorSet.reset(new Wolf::DescriptorSet(linesDescriptorSetLayout->getDescriptorSetLayout(), Wolf::UpdateRate::NEVER));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(linesDescriptorSetLayoutGenerator->getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *linesUniformBuffer);
	linesDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}