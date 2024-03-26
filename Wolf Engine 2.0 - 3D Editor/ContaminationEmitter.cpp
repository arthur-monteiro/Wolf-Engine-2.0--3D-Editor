#include "ContaminationEmitter.h"

#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"

ContaminationEmitter::ContaminationEmitter(const std::function<void(ComponentInterface*)>& requestReloadCallback)
{
	m_requestReloadCallback = requestReloadCallback;

	Wolf::CreateImageInfo createImageInfo{};
	createImageInfo.extent = { CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE };
	createImageInfo.format = VK_FORMAT_R8_UINT;
	createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	createImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	createImageInfo.mipLevelCount = 1;
	m_contaminationIdsImage.reset(new Wolf::Image(createImageInfo));
	m_contaminationIdsImage->setImageLayout(Wolf::Image::SampledInFragmentShader());

	m_descriptorSetLayoutGenerator.addCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	m_descriptorSetLayout.reset(new Wolf::DescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
	m_sampler.reset(new Wolf::Sampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_NEAREST));
	descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_contaminationIdsImage->getDefaultImageView(), *m_sampler);

	m_descriptorSet.reset(new Wolf::DescriptorSet(m_descriptorSetLayout->getDescriptorSetLayout(), Wolf::UpdateRate::NEVER));
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	buildDebugMesh();
	m_debugMesh.reset(m_newDebugMesh.release());
}

void ContaminationEmitter::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<ContaminationMaterial>(jsonReader, ID, m_savedEditorParams);
}

void ContaminationEmitter::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void ContaminationEmitter::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void ContaminationEmitter::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	if (static_cast<bool>(m_drawDebug))
	{
		if (m_newDebugMesh)
			m_debugMesh.reset(m_newDebugMesh.release());

		const bool useNewMesh = m_debugMeshRebuildRequested;
		if (m_debugMeshRebuildRequested)
			buildDebugMesh();

		DebugRenderingManager::LinesUBData uniformData{};
		uniformData.transform = glm::mat4(1.0f);
		debugRenderingManager.addCustomGroupOfLines(useNewMesh ? m_newDebugMesh.createNonOwnerResource() : m_debugMesh.createNonOwnerResource(), uniformData);
	}
}

ContaminationEmitter::ContaminationMaterial::ContaminationMaterial() : ParameterGroupInterface(ContaminationEmitter::TAB), m_material(ContaminationEmitter::TAB, "")
{
	m_name = DEFAULT_NAME;
}

std::span<EditorParamInterface*> ContaminationEmitter::ContaminationMaterial::getAllParams()
{
	return m_material.getAllParams();
}

std::span<EditorParamInterface* const> ContaminationEmitter::ContaminationMaterial::getAllConstParams() const
{
	return m_material.getAllConstParams();
}

bool ContaminationEmitter::ContaminationMaterial::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

void ContaminationEmitter::onMaterialAdded()
{
	m_contaminationMaterials.back().subscribe(this, [this]() { /* nothing to do */ });
	m_requestReloadCallback(this);
}

void ContaminationEmitter::onFillSceneWithValueChanged() const
{
	if (m_fillSceneWithValue.isEnabled())
	{
		const uint8_t value = static_cast<uint8_t>(m_fillSceneWithValue);
		const std::vector<uint8_t> bufferWithValue(static_cast<size_t>(CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE), value);

		m_contaminationIdsImage->copyCPUBuffer(bufferWithValue.data(), Wolf::Image::SampledInFragmentShader());
	}
}

void ContaminationEmitter::requestDebugMeshRebuild()
{
	m_debugMeshRebuildRequested = true;
}

void ContaminationEmitter::buildDebugMesh()
{
	const glm::vec3 cellSize = glm::vec3(static_cast<float>(CONTAMINATION_IDS_IMAGE_SIZE) / static_cast<float>(m_size));

	std::vector<DebugRenderingManager::LineVertex> vertices;
	std::vector<uint32_t> indices;

	// Normal X
	for (uint32_t y = 0; y < CONTAMINATION_IDS_IMAGE_SIZE; ++y)
	{
		for (uint32_t z = 0; z < CONTAMINATION_IDS_IMAGE_SIZE; ++z)
		{
			glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(0, y, z) * cellSize;
			vertices.emplace_back(firstPos);
			vertices.emplace_back(firstPos + glm::vec3(m_size, 0, 0));
		}
	}

	// Normal Y
	for (uint32_t x = 0; x < CONTAMINATION_IDS_IMAGE_SIZE; ++x)
	{
		for (uint32_t z = 0; z < CONTAMINATION_IDS_IMAGE_SIZE; ++z)
		{
			glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(x, 0, z) * cellSize;
			vertices.emplace_back(firstPos);
			vertices.emplace_back(firstPos + glm::vec3(0, m_size, 0));
		}
	}

	// Normal Z
	for (uint32_t x = 0; x < CONTAMINATION_IDS_IMAGE_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CONTAMINATION_IDS_IMAGE_SIZE; ++y)
		{
			glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(x, y, 0) * cellSize;
			vertices.emplace_back(firstPos);
			vertices.emplace_back(firstPos + glm::vec3(0, 0, m_size));
		}
	}

	for (uint32_t i = 0; i < vertices.size(); ++i)
	{
		indices.push_back(i);
	}

	m_newDebugMesh.reset(new Wolf::Mesh(vertices, indices));

	m_debugMeshRebuildRequested = false;
}
