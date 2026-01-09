#include "ContaminationEmitter.h"

#include <fstream>
#include <random>

#include "ContaminationMaterial.h"
#include "ContaminationReceiver.h"
#include "ContaminationUpdatePass.h"
#include "DebugRenderingManager.h"
#include "EditorModelInterface.h"
#include "EditorParamsHelper.h"
#include "MaterialComponent.h"
#include "RenderingPipelineInterface.h"
#include "UpdateGPUBuffersPass.h"

ContaminationEmitter::ContaminationEmitter(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
                                           const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback, 
                                           const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager)
	: m_materialGPUManager(materialsGPUManager), m_editorConfiguration(editorConfiguration), m_contaminationUpdatePass(renderingPipeline->getContaminationUpdatePass()), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback),
      m_physicsManager(physicsManager), m_updateGPUBuffersPass(renderingPipeline->getUpdateGPUBuffersPass())
{
	m_requestReloadCallback = requestReloadCallback;

	Wolf::CreateImageInfo createImageInfo{};
	createImageInfo.extent = { CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE, CONTAMINATION_IDS_IMAGE_SIZE };
	createImageInfo.format = Wolf::Format::R8_UINT;
	createImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::COLOR;
	createImageInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::STORAGE;
	createImageInfo.mipLevelCount = 1;
	m_contaminationIdsImage.reset(Wolf::Image::createImage(createImageInfo));
	m_contaminationIdsImage->setImageLayout(Wolf::Image::SampledInFragmentShader());

	m_descriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
	m_descriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 1);
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

	m_activatedCells.fill(true);
}

ContaminationEmitter::~ContaminationEmitter()
{
	m_contaminationUpdatePass->unregisterEmitter(this);
}

void ContaminationEmitter::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<ContaminationMaterialArrayItem<TAB>>(jsonReader, ID, m_savedEditorParams);
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

void ContaminationEmitter::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	if (m_transferInfoToBufferRequested)
		transferInfoToBuffer();

	for (uint32_t i = 0; i < m_contaminationMaterials.size(); ++i)
	{
		ContaminationMaterialArrayItem<TAB>& materialEditor = m_contaminationMaterials[i];
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

void ContaminationEmitter::saveCustom() const
{
	std::string activatedCellsSaveFile = m_editorConfiguration->computeFullPathFromLocalPath(m_entity->getLoadingPath() + ".contamination_activate_cells.bin");

	std::ofstream fs(activatedCellsSaveFile, std::ios::out | std::ios::binary);
	uint32_t contaminationIdsImageSize = CONTAMINATION_IDS_IMAGE_SIZE;
	fs.write(reinterpret_cast<char*>(&contaminationIdsImageSize), sizeof(uint32_t));
	fs.write(reinterpret_cast<const char*>(m_activatedCells.data()), sizeof(bool) * m_activatedCells.size());
	fs.close();
}

void ContaminationEmitter::addShootRequest(ShootRequest shootRequest)
{
	m_shootRequestLock.lock();
	m_shootRequests.emplace_back(shootRequest);
	m_shootRequestLock.unlock();
}

void ContaminationEmitter::onEntityRegistered()
{
	std::string activatedCellsSaveFile = m_editorConfiguration->computeFullPathFromLocalPath(m_entity->getLoadingPath() + ".contamination_activate_cells.bin");
	std::ifstream input(activatedCellsSaveFile, std::ios::in | std::ios::binary);

	uint32_t contaminationIdsImageSize;
	input.read(reinterpret_cast<char*>(&contaminationIdsImageSize), sizeof(contaminationIdsImageSize));

	if (contaminationIdsImageSize != CONTAMINATION_IDS_IMAGE_SIZE)
	{
		Wolf::Debug::sendWarning("Contamination emitter save corrupted or outdated, size is not same");
		input.close();
		return;
	}

	input.read(reinterpret_cast<char*>(m_activatedCells.data()), sizeof(bool) * m_activatedCells.size());

	input.close();
}

void ContaminationEmitter::onMaterialAdded()
{
	m_contaminationMaterials.back().setGetEntityFromLoadingPathCallback(m_getEntityFromLoadingPathCallback);
	m_contaminationMaterials.back().subscribe(this, [this](Flags) { onParamChanged(); });
	m_contaminationMaterials.back().setIndex(static_cast<uint32_t>(m_contaminationMaterials.size()) - 1);
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
	for (uint32_t y = 0; y < CONTAMINATION_IDS_IMAGE_SIZE; ++y)
	{
		for (uint32_t z = 0; z < CONTAMINATION_IDS_IMAGE_SIZE; ++z)
		{
			for (float offset = 0.0f; offset < m_size; offset += cellSize.x)
			{

				if (m_activatedCells[static_cast<uint32_t>(offset / cellSize.x) + y * CONTAMINATION_IDS_IMAGE_SIZE + z * (CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE)])
				{
					glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(0, y, z) * cellSize;
					vertices.emplace_back(firstPos + glm::vec3(offset, 0, 0));
					vertices.emplace_back(firstPos + glm::vec3(offset + cellSize.x, 0, 0));
				}
			}
		}
	}

	// Normal Y
	for (uint32_t x = 0; x < CONTAMINATION_IDS_IMAGE_SIZE; ++x)
	{
		for (uint32_t z = 0; z < CONTAMINATION_IDS_IMAGE_SIZE; ++z)
		{
			for (float offset = 0.0f; offset < m_size; offset += cellSize.x)
			{
				if (m_activatedCells[x + static_cast<uint32_t>(offset / cellSize.x) * CONTAMINATION_IDS_IMAGE_SIZE + z * (CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE)])
				{
					glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(x, 0, z) * cellSize;
					vertices.emplace_back(firstPos + glm::vec3(0, offset, 0));
					vertices.emplace_back(firstPos + glm::vec3(0, offset + cellSize.x, 0));
				}
			}
		}
	}

	// Normal Z
	for (uint32_t x = 0; x < CONTAMINATION_IDS_IMAGE_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CONTAMINATION_IDS_IMAGE_SIZE; ++y)
		{
			for (float offset = 0.0f; offset < m_size; offset += cellSize.x)
			{
				if (m_activatedCells[x + y * CONTAMINATION_IDS_IMAGE_SIZE + static_cast<uint32_t>(offset / cellSize.x) * (CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE)])
				{
					glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(x, y, 0) * cellSize;
					vertices.emplace_back(firstPos + glm::vec3(0, 0, offset));
					vertices.emplace_back(firstPos + glm::vec3(0, 0, offset + cellSize.x));
				}
			}
		}
	}

	if (vertices.size() < 2)
	{
		vertices.resize(2);
		vertices.emplace_back(glm::vec3(0.0f));
		vertices.emplace_back(glm::vec3(0.0f));
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
	ContaminationInfo contaminationInfo{ m_offset, m_size,  m_size / static_cast<float>(CONTAMINATION_IDS_IMAGE_SIZE) };
	for (uint32_t i = 0; i < m_contaminationMaterials.size(); ++i)
	{
		contaminationInfo.materialIds[i] = m_contaminationMaterials[i].getMaterialIdx();
	}

	UpdateGPUBuffersPass::Request request(&contaminationInfo, sizeof(ContaminationInfo), m_contaminationInfoBuffer.createNonOwnerResource(), 0);
	m_updateGPUBuffersPass->addRequestBeforeFrame(request);

	m_transferInfoToBufferRequested = false;
}

void ContaminationEmitter::buildEmitter()
{
	const glm::vec3 cellSize = glm::vec3(static_cast<float>(m_size) / static_cast<float>(CONTAMINATION_IDS_IMAGE_SIZE));
	for (uint32_t x = 0; x < CONTAMINATION_IDS_IMAGE_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CONTAMINATION_IDS_IMAGE_SIZE; ++y)
		{
			for (uint32_t z = 0; z < CONTAMINATION_IDS_IMAGE_SIZE; ++z)
			{
				bool& isActivated = m_activatedCells[x + y * CONTAMINATION_IDS_IMAGE_SIZE + z * (CONTAMINATION_IDS_IMAGE_SIZE * CONTAMINATION_IDS_IMAGE_SIZE)];
				isActivated = false;

				glm::vec3 firstPos = static_cast<glm::vec3>(m_offset) + glm::vec3(x, y, z) * cellSize;
				glm::vec3 lastPos = static_cast<glm::vec3>(m_offset) + glm::vec3(x + 1, y + 1, z + 1) * cellSize;

				Wolf::Physics::PhysicsManager::RayCastResult rayCastResult = m_physicsManager->rayCastAnyHit(firstPos - cellSize * 0.01f, lastPos + cellSize * 0.01f);
				if (rayCastResult.collision && rayCastResult.instance)
				{
					if (static_cast<Entity*>(rayCastResult.instance)->getComponent<ContaminationReceiver>())
					{
						isActivated = true;
					}
				}
			}
		}
	}

	m_debugMeshRebuildRequested = true;
}
