#include "DebugRenderingManager.h"

#include <glm/gtc/matrix_transform.hpp>

#include <Debug.h>
#include <DescriptorSetLayoutGenerator.h>
#include <RenderMeshList.h>

#include "CommonDescriptorLayouts.h"
#include "DescriptorSetGenerator.h"

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

void DebugRenderingManager::updateGraphic()
{
	if (m_perGroupOfLinesInfoArray.size() < m_entitiesForAABBDraw.size())
	{
		const uint32_t previousPerGroupOfLinesInfoArraySize = static_cast<uint32_t>(m_perGroupOfLinesInfoArray.size());
		m_perGroupOfLinesInfoArray.resize(m_entitiesForAABBDraw.size());

		for (uint32_t i = previousPerGroupOfLinesInfoArraySize; i < m_perGroupOfLinesInfoArray.size(); ++i)
		{
			m_perGroupOfLinesInfoArray[i].linesUniformBuffer.reset(new Wolf::Buffer(sizeof(LinesUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Wolf::UpdateRate::NEVER));
			m_perGroupOfLinesInfoArray[i].linesDescriptorSet.reset(new Wolf::DescriptorSet(m_linesDescriptorSetLayout->getDescriptorSetLayout(), Wolf::UpdateRate::NEVER));

			Wolf::DescriptorSetGenerator descriptorSetGenerator(m_linesDescriptorSetLayoutGenerator->getDescriptorLayouts());
			descriptorSetGenerator.setBuffer(0, *m_perGroupOfLinesInfoArray[i].linesUniformBuffer);
			m_perGroupOfLinesInfoArray[i].linesDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
		}
	}

	for (uint32_t i = 0; i < m_entitiesForAABBDraw.size(); ++i)
	{
		const PerGroupOfLines& perGroupOfLinesInfo = m_perGroupOfLinesInfoArray[i];
		Wolf::AABB entityAABB = m_entitiesForAABBDraw[i]->getAABB();

		LinesUBData data{};
		data.transform = glm::translate(glm::mat4(glm::mat4(1.0f)), glm::vec3(entityAABB.getCenter()));
		data.transform = glm::scale(data.transform, glm::vec3(entityAABB.getSize()));
		perGroupOfLinesInfo.linesUniformBuffer->transferCPUMemory(&data, sizeof(data));
	}
}

void DebugRenderingManager::addMeshesToRenderList(Wolf::RenderMeshList& renderMeshList)
{
	for (auto& perGroupOfLinesInfo : m_perGroupOfLinesInfoArray)
	{
		Wolf::RenderMeshList::MeshToRenderInfo meshToRenderInfo(m_AABBMesh.get(), m_linesPipelineSet.get());
		meshToRenderInfo.descriptorSets.push_back({ perGroupOfLinesInfo.linesDescriptorSet.createConstNonOwnerResource(), 1});

		renderMeshList.addMeshToRender(meshToRenderInfo);
	}
}

void DebugRenderingManager::setSelectedEntity(const Wolf::ResourceNonOwner<Entity>& entity)
{
	m_entitiesForAABBDraw.clear();

	if (entity->hasModelComponent())
		m_entitiesForAABBDraw.push_back(entity);
}