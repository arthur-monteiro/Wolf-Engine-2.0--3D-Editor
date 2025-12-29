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
		m_linesDescriptorSetLayoutGenerator->addUniformBuffer(Wolf::ShaderStageFlagBits::VERTEX, 0); // transform

		m_linesDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_linesDescriptorSetLayoutGenerator->getDescriptorLayouts()));

		m_linesPipelineSet.reset(new Wolf::PipelineSet);

		Wolf::PipelineSet::PipelineInfo pipelineInfo;

		pipelineInfo.shaderInfos.resize(2);
		pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/debug/lines.vert";
		pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;
		pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/debug/lines.frag";
		pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

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
		m_cubeQuadMesh.reset(new Wolf::Mesh(cubeVertices, cubeQuadIndices));
	}

	// Spheres
	{
		m_filledSphereData.init(Wolf::PolygonMode::FILL);
		m_wiredSphereData.init(Wolf::PolygonMode::LINE);
	}

	// Rectangles
	{
		const std::vector<DebugVertex> rectangleVertices =
		{
			{ glm::vec3(-0.5, -0.5f, 0.0f)},
			{ glm::vec3(0.5f, -0.5f, 0.0f) },
			{ glm::vec3(-0.5f, 0.5f, 0.0f) },
			{ glm::vec3(0.5f, 0.5f, 0.0f) },
		};

		const std::vector<uint32_t> rectangleIndices =
		{
			0, 1, 3,
			0, 3, 2
		};

		m_rectangleMesh.reset(new Wolf::Mesh(rectangleVertices, rectangleIndices));

		m_rectanglesDescriptorSetLayoutGenerator.reset(new Wolf::DescriptorSetLayoutGenerator);
		m_rectanglesDescriptorSetLayoutGenerator->addUniformBuffer(Wolf::ShaderStageFlagBits::VERTEX, 0);

		m_rectanglesDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_rectanglesDescriptorSetLayoutGenerator->getDescriptorLayouts()));

		m_rectanglesPipelineSet.reset(new Wolf::PipelineSet);

		Wolf::PipelineSet::PipelineInfo pipelineInfo;

		pipelineInfo.shaderInfos.resize(2);
		pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/debug/rectangle.vert";
		pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;
		pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/debug/rectangle.frag";
		pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

		// IA
		DebugVertex::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

		pipelineInfo.vertexInputBindingDescriptions.resize(1);
		DebugVertex::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

		pipelineInfo.cullMode = VK_CULL_MODE_NONE;

		// Resources
		pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

		// Color Blend
		pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

		// Dynamic states
		pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		m_rectanglesPipelineSet->addEmptyPipeline(CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
		const uint32_t pipelineIdx = m_rectanglesPipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
		if (pipelineIdx != CommonPipelineIndices::PIPELINE_IDX_FORWARD)
			Wolf::Debug::sendError("Unexpected pipeline idx");

		m_rectanglesDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_rectanglesDescriptorSetLayout));

		Wolf::DescriptorSetGenerator descriptorSetGenerator(m_rectanglesDescriptorSetLayoutGenerator->getDescriptorLayouts());
		m_rectanglesUniformBuffer.reset(new Wolf::UniformBuffer(sizeof(RectanglesUBData)));
		descriptorSetGenerator.setUniformBuffer(0, *m_rectanglesUniformBuffer);
		m_rectanglesDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
	}

	m_customLinesInfoArray.resize(Wolf::g_configuration->getMaxCachedFrames());
	m_customLinesInfoArrayCount.resize(Wolf::g_configuration->getMaxCachedFrames());
	for (uint32_t& customLineCount : m_customLinesInfoArrayCount)
		customLineCount = 0;
}

void DebugRenderingManager::clearAll()
{
	for (uint32_t queueIdx = 0; queueIdx < Wolf::g_configuration->getMaxCachedFrames(); ++queueIdx)
	{
		clearForQueueIdx(queueIdx);
	}
}

void DebugRenderingManager::clearBeforeFrame()
{
	uint32_t requestQueueIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();

	clearForQueueIdx(requestQueueIdx);
}

void DebugRenderingManager::addAABB(const Wolf::AABB& box)
{
	addBox(box, glm::vec3(0.0f));
}

void DebugRenderingManager::addCustomGroupOfLines(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh, const LinesUBData& data)
{
	uint32_t requestQueueIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();

	m_customLinesMutex.lock();

	if (m_customLinesInfoArrayCount[requestQueueIdx] >= m_customLinesInfoArray[requestQueueIdx].size())
	{
		m_customLinesInfoArray[requestQueueIdx].emplace_back(mesh, m_linesDescriptorSetLayout, m_linesDescriptorSetLayoutGenerator);
	}
	m_customLinesInfoArray[requestQueueIdx][m_customLinesInfoArrayCount[requestQueueIdx]].mesh = mesh;
	m_customLinesInfoArray[requestQueueIdx][m_customLinesInfoArrayCount[requestQueueIdx]].linesUniformBuffer->transferCPUMemory(&data, sizeof(data));
	m_customLinesInfoArrayCount[requestQueueIdx]++;

	m_customLinesMutex.unlock();
}

void DebugRenderingManager::addSphere(const glm::vec3& worldPos, float radius, const glm::vec3& color)
{
	m_filledSphereData.add(worldPos, radius, color);
}

void DebugRenderingManager::addWiredSphere(const glm::vec3& worldPos, float radius, const glm::vec3& color)
{
	m_wiredSphereData.add(worldPos, radius, color);
}

void DebugRenderingManager::addRectangle(const glm::mat4& transform)
{
	if (m_rectanglesCount >= MAX_RECTANGLES_COUNT)
	{
		Wolf::Debug::sendError("Max rectangle count reached");
		return;
	}

	m_rectanglesData.transform[m_rectanglesCount] = transform;
	m_rectanglesCount++;
}

void DebugRenderingManager::addBox(const Wolf::AABB& aabb, const glm::vec3& rotation)
{
	if (m_AABBInfoArrayCount >= m_AABBInfoArray.size())
	{
		m_AABBInfoArray.emplace_back(m_cubeLineMesh.createNonOwnerResource(), m_linesDescriptorSetLayout, m_linesDescriptorSetLayoutGenerator);
	}

	const PerGroupOfLines& perGroupOfLinesInfo = m_AABBInfoArray[m_AABBInfoArrayCount];

	LinesUBData data{};
	data.transform = glm::translate(glm::mat4(glm::mat4(1.0f)), glm::vec3(aabb.getCenter()));
	data.transform = glm::rotate(data.transform, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	data.transform = glm::rotate(data.transform, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	data.transform = glm::rotate(data.transform, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
	data.transform = glm::scale(data.transform, glm::vec3(aabb.getSize()));
	perGroupOfLinesInfo.linesUniformBuffer->transferCPUMemory(&data, sizeof(data));

	m_AABBInfoArrayCount++;
}

void DebugRenderingManager::addMeshesToRenderList(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList)
{
	PROFILE_FUNCTION

	uint32_t requestQueueIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();

	m_meshesToRender.clear();

	if (!g_editorConfiguration->getEnableDebugDraw())
		return;

	for (uint32_t i = 0; i < m_AABBInfoArrayCount; ++i)
	{
		PerGroupOfLines& AABBInfo = m_AABBInfoArray[i];

		Wolf::RenderMeshList::MeshToRender meshToRenderInfo = { AABBInfo.mesh.duplicateAs<Wolf::MeshInterface>(), m_linesPipelineSet.createConstNonOwnerResource() };
		meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(AABBInfo.linesDescriptorSet.createConstNonOwnerResource(), 
			m_linesDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MESH_DEBUG);

		renderMeshList->addTransientMesh(meshToRenderInfo);
	}

	for (uint32_t i = 0; i < m_customLinesInfoArrayCount[requestQueueIdx]; ++i)
	{
		PerGroupOfLines& customInfo = m_customLinesInfoArray[requestQueueIdx][i];

		Wolf::RenderMeshList::MeshToRender meshToRenderInfo = { customInfo.mesh.duplicateAs<Wolf::MeshInterface>(), m_linesPipelineSet.createConstNonOwnerResource() };
		meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(customInfo.linesDescriptorSet.createConstNonOwnerResource(), 
			m_linesDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MESH_DEBUG);

		renderMeshList->addTransientMesh(meshToRenderInfo);
	}

	m_filledSphereData.registerMeshes(renderMeshList, m_cubeQuadMesh);
	m_wiredSphereData.registerMeshes(renderMeshList, m_cubeQuadMesh);

	if (m_rectanglesCount > 0)
	{
		m_rectanglesUniformBuffer->transferCPUMemory(&m_rectanglesData, sizeof(RectanglesUBData));

		Wolf::RenderMeshList::InstancedMesh meshToRenderInfo = { {m_rectangleMesh.createNonOwnerResource<Wolf::MeshInterface>(), m_rectanglesPipelineSet.createConstNonOwnerResource() }, Wolf::NullableResourceNonOwner<Wolf::Buffer>() };
		meshToRenderInfo.mesh.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(m_rectanglesDescriptorSet.createConstNonOwnerResource(),
			m_rectanglesDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MESH_DEBUG);

		renderMeshList->addTransientInstancedMesh(meshToRenderInfo, m_rectanglesCount);
	}
}

void DebugRenderingManager::clearForQueueIdx(uint32_t queueIndex)
{
	m_AABBInfoArrayCount = 0;
	for (uint32_t i = 0; i < m_customLinesInfoArrayCount[queueIndex]; ++i)
		m_customLinesInfoArray[queueIndex][i].mesh = m_cubeLineMesh.createNonOwnerResource(); // stop using the custom mesh as it can be deleted
	m_customLinesInfoArrayCount[queueIndex] = 0;
	m_filledSphereData.clear();
	m_wiredSphereData.clear();
	m_rectanglesCount = 0;
}

DebugRenderingManager::PerGroupOfLines::PerGroupOfLines(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh,
                                                        const Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& linesDescriptorSetLayout,
                                                        const std::unique_ptr<Wolf::DescriptorSetLayoutGenerator>& linesDescriptorSetLayoutGenerator) : mesh(mesh)
{
	linesUniformBuffer.reset(new Wolf::UniformBuffer(sizeof(LinesUBData)));
	linesDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*linesDescriptorSetLayout));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(linesDescriptorSetLayoutGenerator->getDescriptorLayouts());
	descriptorSetGenerator.setUniformBuffer(0, *linesUniformBuffer);
	linesDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void DebugRenderingManager::PerSpherePipeline::init(Wolf::PolygonMode polygonMode)
{
	m_descriptorSetLayoutGenerator.reset(new Wolf::DescriptorSetLayoutGenerator);
	m_descriptorSetLayoutGenerator->addUniformBuffer(Wolf::ShaderStageFlagBits::TESSELLATION_EVALUATION | Wolf::ShaderStageFlagBits::FRAGMENT, 0);

	m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator->getDescriptorLayouts()));

	m_pipelineSet.reset(new Wolf::PipelineSet);

	Wolf::PipelineSet::PipelineInfo pipelineInfo;

	pipelineInfo.shaderInfos.resize(4);
	pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/debug/sphere.vert";
	pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;
	pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/debug/sphere.tesc";
	pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::TESSELLATION_CONTROL;
	pipelineInfo.shaderInfos[2].shaderFilename = "Shaders/debug/sphere.tese";
	pipelineInfo.shaderInfos[2].stage = Wolf::ShaderStageFlagBits::TESSELLATION_EVALUATION;
	pipelineInfo.shaderInfos[3].shaderFilename = "Shaders/debug/sphere.frag";
	pipelineInfo.shaderInfos[3].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

	// IA
	DebugVertex::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

	pipelineInfo.vertexInputBindingDescriptions.resize(1);
	DebugVertex::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

	pipelineInfo.polygonMode = polygonMode;
	pipelineInfo.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	pipelineInfo.patchControlPoint = 4;
	pipelineInfo.cullMode = VK_CULL_MODE_NONE;

	// Resources
	pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

	// Color Blend
	pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

	// Dynamic states
	pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

	m_pipelineSet->addEmptyPipeline(CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
	const uint32_t pipelineIdx = m_pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
	if (pipelineIdx != CommonPipelineIndices::PIPELINE_IDX_FORWARD)
		Wolf::Debug::sendError("Unexpected pipeline idx");

	m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator->getDescriptorLayouts());
	m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(SpheresUBData)));
	descriptorSetGenerator.setUniformBuffer(0, *m_uniformBuffer);
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void DebugRenderingManager::PerSpherePipeline::add(const glm::vec3& worldPos, float radius, const glm::vec3& color)
{
	if (m_count >= MAX_SPHERE_COUNT)
	{
		Wolf::Debug::sendError("Max sphere count reached");
		return;
	}

	m_uniformData.worldPosAndRadius[m_count] = glm::vec4(worldPos, radius);
	m_uniformData.colors[m_count] = glm::vec4(color, 1.0f);
	m_count++;
}

void DebugRenderingManager::PerSpherePipeline::registerMeshes(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList, Wolf::ResourceUniqueOwner<Wolf::Mesh>& cubeQuadMesh)
{
	if (m_count > 0)
	{
		m_uniformBuffer->transferCPUMemory(&m_uniformData, sizeof(SpheresUBData));

		Wolf::RenderMeshList::InstancedMesh meshToRenderInfo = { {cubeQuadMesh.createNonOwnerResource<Wolf::MeshInterface>(), m_pipelineSet.createConstNonOwnerResource() }, Wolf::NullableResourceNonOwner<Wolf::Buffer>() };
		meshToRenderInfo.mesh.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(m_descriptorSet.createConstNonOwnerResource(),
			m_descriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MESH_DEBUG);

		renderMeshList->addTransientInstancedMesh(meshToRenderInfo, m_count);
	}
}

void DebugRenderingManager::PerSpherePipeline::clear()
{
	m_count = 0;
}
