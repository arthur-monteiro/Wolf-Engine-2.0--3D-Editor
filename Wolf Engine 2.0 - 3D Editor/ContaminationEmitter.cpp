#include "ContaminationEmitter.h"

#include <random>

#include "ContaminationUpdatePass.h"
#include "DebugRenderingManager.h"
#include "EditorModelInterface.h"
#include "EditorParamsHelper.h"
#include "MaterialComponent.h"
#include "RenderingPipelineInterface.h"

ContaminationEmitter::ContaminationEmitter(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
                                           const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
	: m_materialGPUManager(materialsGPUManager), m_editorConfiguration(editorConfiguration), m_contaminationUpdatePass(renderingPipeline->getContaminationUpdatePass()), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
	m_requestReloadCallback = requestReloadCallback;

	Wolf::CreateImageInfo createImageInfo{};
	createImageInfo.extent = { CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE };
	createImageInfo.format = VK_FORMAT_R8_UINT;
	createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	createImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	createImageInfo.mipLevelCount = 1;
	m_contaminationIdsImage.reset(Wolf::Image::createImage(createImageInfo));
	m_contaminationIdsImage->setImageLayout(Wolf::Image::SampledInFragmentShader());

	m_descriptorSetLayoutGenerator.addCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	m_descriptorSetLayoutGenerator.addStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());

	m_sampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_NEAREST));
	descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_contaminationIdsImage->getDefaultImageView(), *m_sampler);

	m_contaminationInfoBuffer.reset(Wolf::Buffer::createBuffer(sizeof(ContaminationInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	descriptorSetGenerator.setBuffer(1, *m_contaminationInfoBuffer);

	m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	buildDebugMesh();
	m_debugMesh.reset(m_newDebugMesh.release());

	m_contaminationUpdatePass->registerEmitter(this);
}

ContaminationEmitter::~ContaminationEmitter()
{
	m_contaminationUpdatePass->unregisterEmitter(this);
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

void ContaminationEmitter::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
	if (m_transferInfoToBufferRequested)
		transferInfoToBuffer();

	for (uint32_t i = 0; i < m_contaminationMaterials.size(); ++i)
	{
		ContaminationMaterial& materialEditor = m_contaminationMaterials[i];
		materialEditor.updateBeforeFrame(m_materialGPUManager, m_editorConfiguration);
	}

	m_shootRequestLock.lock();
	m_contaminationUpdatePass->swapShootRequests(m_shootRequests);
	m_shootRequestLock.unlock();
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

void ContaminationEmitter::addShootRequest(ShootRequest shootRequest)
{
	m_shootRequestLock.lock();
	m_shootRequests.emplace_back(shootRequest);
	m_shootRequestLock.unlock();
}

ContaminationEmitter::ContaminationMaterial::ContaminationMaterial() : ParameterGroupInterface(ContaminationEmitter::TAB)
{
	m_name = DEFAULT_NAME;
}

void ContaminationEmitter::ContaminationMaterial::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	if (m_materialEntity && !m_materialNotificationRegistered)
	{
		if (const Wolf::ResourceNonOwner<MaterialComponent> materialComponent = (*m_materialEntity)->getComponent<MaterialComponent>())
		{
			materialComponent->subscribe(this, [this]() { notifySubscribers(); });
			m_materialNotificationRegistered = true;

			notifySubscribers();
		}
	}
}

void ContaminationEmitter::ContaminationMaterial::setGetEntityFromLoadingPathCallback(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
{
	m_getEntityFromLoadingPathCallback = getEntityFromLoadingPathCallback;
}

void ContaminationEmitter::ContaminationMaterial::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

void ContaminationEmitter::ContaminationMaterial::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

bool ContaminationEmitter::ContaminationMaterial::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

uint32_t ContaminationEmitter::ContaminationMaterial::getMaterialIdx() const
{
	if (m_materialEntity)
	{
		if (const Wolf::ResourceNonOwner<MaterialComponent> materialComponent = (*m_materialEntity)->getComponent<MaterialComponent>())
		{
			return materialComponent->getMaterialIdx();
		}
	}

	return 0;
}

void ContaminationEmitter::ContaminationMaterial::onMaterialEntityChanged()
{
	if (static_cast<std::string>(m_materialEntityParam).empty())
	{
		m_materialEntity.reset(nullptr);
		return;
	}

	m_materialEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_materialEntityParam)));
	m_materialNotificationRegistered = false;
}

void ContaminationEmitter::onMaterialAdded()
{
	m_contaminationMaterials.back().setGetEntityFromLoadingPathCallback(m_getEntityFromLoadingPathCallback);
	m_contaminationMaterials.back().subscribe(this, [this]() { onParamChanged(); });
	m_requestReloadCallback(this);
}

void ContaminationEmitter::onFillSceneWithValueChanged() const
{
	if (m_fillWithRandom)
	{
		std::vector<uint8_t> bufferWithValues(static_cast<size_t>(CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE));

		std::random_device dev;
		std::mt19937 rng(dev());
		std::uniform_int_distribution<std::mt19937::result_type> dist(0, static_cast<uint32_t>(m_contaminationMaterials.size()));

		for (uint8_t& bufferValue : bufferWithValues)
		{
			bufferValue = static_cast<uint8_t>(dist(rng));
		}

		m_contaminationIdsImage->copyCPUBuffer(bufferWithValues.data(), Wolf::Image::SampledInFragmentShader());
	}
	else if (m_fillSceneWithValue.isEnabled())
	{
		const uint8_t value = static_cast<uint8_t>(m_fillSceneWithValue);
		const std::vector<uint8_t> bufferWithValue(static_cast<size_t>(CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE), value);

		m_contaminationIdsImage->copyCPUBuffer(bufferWithValue.data(), Wolf::Image::SampledInFragmentShader());
	}
}

void ContaminationEmitter::onParamChanged()
{
	m_debugMeshRebuildRequested = true;
	m_transferInfoToBufferRequested = true;
}

void ContaminationEmitter::buildDebugMesh()
{
	const glm::vec3 cellSize = glm::vec3(static_cast<float>(m_size) / static_cast<float>(CONTAMINATION_IDS_IMAGE_SIZE));

	std::vector<DebugRenderingManager::DebugVertex> vertices;
	std::vector<uint32_t> indices;

	// Normal X
	for (uint32_t y = 0; y <= CONTAMINATION_IDS_IMAGE_SIZE; ++y)
	{
		for (uint32_t z = 0; z <= CONTAMINATION_IDS_IMAGE_SIZE; ++z)
		{
			glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(0, y, z) * cellSize;
			vertices.emplace_back(firstPos);
			vertices.emplace_back(firstPos + glm::vec3(m_size, 0, 0));
		}
	}

	// Normal Y
	for (uint32_t x = 0; x <= CONTAMINATION_IDS_IMAGE_SIZE; ++x)
	{
		for (uint32_t z = 0; z <= CONTAMINATION_IDS_IMAGE_SIZE; ++z)
		{
			glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(x, 0, z) * cellSize;
			vertices.emplace_back(firstPos);
			vertices.emplace_back(firstPos + glm::vec3(0, m_size, 0));
		}
	}

	// Normal Z
	for (uint32_t x = 0; x <= CONTAMINATION_IDS_IMAGE_SIZE; ++x)
	{
		for (uint32_t y = 0; y <= CONTAMINATION_IDS_IMAGE_SIZE; ++y)
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

void ContaminationEmitter::transferInfoToBuffer()
{
	ContaminationInfo contaminationInfo{ m_offset, m_size };
	for (uint32_t i = 0; i < m_contaminationMaterials.size(); ++i)
	{
		contaminationInfo.materialOffsets[i] = m_contaminationMaterials[i].getMaterialIdx();
	}
	m_contaminationInfoBuffer->transferCPUMemoryWithStagingBuffer(&contaminationInfo, sizeof(ContaminationInfo));

	m_transferInfoToBufferRequested = false;
}
