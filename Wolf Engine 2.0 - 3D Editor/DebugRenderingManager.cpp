#include "DebugRenderingManager.h"

#include <glm/gtc/matrix_transform.hpp>

#include <Debug.h>
#include <DescriptorSetGenerator.h>
#include <DescriptorSetLayoutGenerator.h>
#include <ProfilerCommon.h>
#include <RenderMeshList.h>

#include "CommonLayouts.h"
#include "EditorConfiguration.h"

DebugRenderingManager::DebugRenderingManager()
{
	// Lines
	{
		m_linesDescriptorSetLayoutGenerator.reset(new Wolf::DescriptorSetLayoutGenerator);
		m_linesDescriptorSetLayoutGenerator->addUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0); // transform

		m_linesDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_linesDescriptorSetLayoutGenerator->getDescriptorLayouts()));

		m_linesPipelineSet.reset(new Wolf::PipelineSet);

		Wolf::PipelineSet::PipelineInfo pipelineInfo;

		pipelineInfo.shaderInfos.resize(2);
		pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/debug/lines.vert";
		pipelineInfo.shaderInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/debug/lines.frag";
		pipelineInfo.shaderInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		// IA
		DebugVertex::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

		pipelineInfo.vertexInputBindingDescriptions.resize(1);
		DebugVertex::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

		pipelineInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

		// Resources
		pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

		// Color Blend
		pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

		// Dynamic states
		pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		m_linesPipelineSet->addEmptyPipeline(CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
		const uint32_t pipelineIdx = m_linesPipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
		if (pipelineIdx != CommonPipelineIndices::PIPELINE_IDX_FORWARD)
			Wolf::Debug::sendError("Unexpected pipeline idx");
	}

	// Cube
	{
		const std::vector<DebugVertex> cubeVertices =
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

		const std::vector<uint32_t> cubeLineIndices =
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

		const std::vector<uint32_t> cubeQuadIndices =
		{
			// Top
			0, 1, 2, 3,

			// Bot
			4, 5, 6, 7,

			// Left
			0, 2, 4, 6,

			// Right
			1, 3, 5, 7,

			// Back
			0, 1, 4, 5,

			// Front
			2, 3, 6, 7
		};

		m_cubeLineMesh.reset(new Wolf::Mesh(cubeVertices, cubeLineIndices));
		m_cubeLQuadMesh.reset(new Wolf::Mesh(cubeVertices, cubeQuadIndices));
	}

	// Spheres
	{
		m_spheresDescriptorSetLayoutGenerator.reset(new Wolf::DescriptorSetLayoutGenerator);
		m_spheresDescriptorSetLayoutGenerator->addUniformBuffer(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0);

		m_spheresDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_spheresDescriptorSetLayoutGenerator->getDescriptorLayouts()));

		m_spheresPipelineSet.reset(new Wolf::PipelineSet);

		Wolf::PipelineSet::PipelineInfo pipelineInfo;

		pipelineInfo.shaderInfos.resize(4);
		pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/debug/sphere.vert";
		pipelineInfo.shaderInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/debug/sphere.tesc";
		pipelineInfo.shaderInfos[1].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		pipelineInfo.shaderInfos[2].shaderFilename = "Shaders/debug/sphere.tese";
		pipelineInfo.shaderInfos[2].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		pipelineInfo.shaderInfos[3].shaderFilename = "Shaders/debug/sphere.frag";
		pipelineInfo.shaderInfos[3].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		// IA
		DebugVertex::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

		pipelineInfo.vertexInputBindingDescriptions.resize(1);
		DebugVertex::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

		pipelineInfo.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		pipelineInfo.patchControlPoint = 4;
		pipelineInfo.cullMode = VK_CULL_MODE_NONE;

		// Resources
		pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

		// Color Blend
		pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

		// Dynamic states
		pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		m_spheresPipelineSet->addEmptyPipeline(CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
		const uint32_t pipelineIdx = m_spheresPipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
		if (pipelineIdx != CommonPipelineIndices::PIPELINE_IDX_FORWARD)
			Wolf::Debug::sendError("Unexpected pipeline idx");

		m_spheresDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_spheresDescriptorSetLayout));

		Wolf::DescriptorSetGenerator descriptorSetGenerator(m_spheresDescriptorSetLayoutGenerator->getDescriptorLayouts());
		m_spheresUniformBuffer.reset(Wolf::Buffer::createBuffer(sizeof(SpheresUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		descriptorSetGenerator.setBuffer(0, *m_spheresUniformBuffer);
		m_spheresDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
	}
}

void DebugRenderingManager::clearBeforeFrame()
{
	m_AABBInfoArrayCount = 0;
	for (uint32_t i = 0; i < m_customLinesInfoArrayCount; ++i)
		m_customLinesInfoArray[i].mesh = m_cubeLineMesh.createNonOwnerResource(); // stop using the custom mesh as it can be deleted
	m_customLinesInfoArrayCount = 0;
	m_sphereCount = 0;
}

void DebugRenderingManager::addAABB(const Wolf::AABB& box)
{
	if (m_AABBInfoArrayCount >= m_AABBInfoArray.size())
	{
		m_AABBInfoArray.emplace_back(m_cubeLineMesh.createNonOwnerResource(), m_linesDescriptorSetLayout, m_linesDescriptorSetLayoutGenerator);
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
	if (m_customLinesInfoArrayCount >= m_customLinesInfoArray.size())
	{
		m_customLinesInfoArray.emplace_back(mesh, m_linesDescriptorSetLayout, m_linesDescriptorSetLayoutGenerator);
	}
	m_customLinesInfoArray[m_customLinesInfoArrayCount].mesh = mesh;
	m_customLinesInfoArray[m_customLinesInfoArrayCount].linesUniformBuffer->transferCPUMemory(&data, sizeof(data));
	m_customLinesInfoArrayCount++;
}

void DebugRenderingManager::addSphere(const glm::vec3& worldPos, float radius)
{
	if (m_sphereCount >= MAX_SPHERE_COUNT)
	{
		Wolf::Debug::sendError("Max sphere count reached");
		return;
	}

	m_spheresData.worldPosAndRadius[m_sphereCount] = glm::vec4(worldPos, radius);
	m_sphereCount++;
}

void DebugRenderingManager::addMeshesToRenderList(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList)
{
	PROFILE_FUNCTION

	m_meshesToRender.clear();

	if (!g_editorConfiguration->getEnableDebugDraw())
		return;

	for (uint32_t i = 0; i < m_AABBInfoArrayCount; ++i)
	{
		PerGroupOfLines& AABBInfo = m_AABBInfoArray[i];

		Wolf::RenderMeshList::MeshToRender meshToRenderInfo = { AABBInfo.mesh, m_linesPipelineSet.createConstNonOwnerResource() };
		meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(AABBInfo.linesDescriptorSet.createConstNonOwnerResource(), 
			m_linesDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MESH_DEBUG);

		renderMeshList->addTransientMesh(meshToRenderInfo);
	}

	for (uint32_t i = 0; i < m_customLinesInfoArrayCount; ++i)
	{
		PerGroupOfLines& customInfo = m_customLinesInfoArray[i];

		Wolf::RenderMeshList::MeshToRender meshToRenderInfo = { customInfo.mesh, m_linesPipelineSet.createConstNonOwnerResource() };
		meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(customInfo.linesDescriptorSet.createConstNonOwnerResource(), 
			m_linesDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MESH_DEBUG);

		renderMeshList->addTransientMesh(meshToRenderInfo);
	}

	if (m_sphereCount > 0)
	{
		m_spheresUniformBuffer->transferCPUMemory(&m_spheresData, sizeof(SpheresUBData));

		Wolf::RenderMeshList::InstancedMesh meshToRenderInfo = { {m_cubeLQuadMesh.createNonOwnerResource(), m_spheresPipelineSet.createConstNonOwnerResource() }, Wolf::NullableResourceNonOwner<Wolf::Buffer>() };
		meshToRenderInfo.mesh.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(m_spheresDescriptorSet.createConstNonOwnerResource(),
			m_spheresDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MESH_DEBUG);

		renderMeshList->addTransientInstancedMesh(meshToRenderInfo, m_sphereCount);
	}
}

DebugRenderingManager::PerGroupOfLines::PerGroupOfLines(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh,
	const Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& linesDescriptorSetLayout,
	const std::unique_ptr<Wolf::DescriptorSetLayoutGenerator>& linesDescriptorSetLayoutGenerator) : mesh(mesh)
{
	linesUniformBuffer.reset(Wolf::Buffer::createBuffer(sizeof(LinesUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	linesDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*linesDescriptorSetLayout));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(linesDescriptorSetLayoutGenerator->getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *linesUniformBuffer);
	linesDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}