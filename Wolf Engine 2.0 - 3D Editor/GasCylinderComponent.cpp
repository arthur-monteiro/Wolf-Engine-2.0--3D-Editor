#include "GasCylinderComponent.h"

#include <glm/gtx/matrix_decompose.hpp>

#include "CommonLayouts.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "MathsUtilsEditor.h"
#include "ModelLoader.h"
#include "StaticModel.h"

GasCylinderComponent::GasCylinderComponent(const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback,
	const std::function<void(ComponentInterface*)>& requestReloadCallback)
	: m_physicsManager(physicsManager), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback), m_requestReloadCallback(requestReloadCallback)
{
	m_descriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
	m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());

	m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformData)));
	descriptorSetGenerator.setUniformBuffer(0, *m_uniformBuffer);

	m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void GasCylinderComponent::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<ContaminationMaterialArrayItem<TAB>>(jsonReader, ID, m_editorParams);
}

void GasCylinderComponent::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void GasCylinderComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void GasCylinderComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	UniformData uniformData{};

	uniformData.color = static_cast<glm::vec3>(m_color);
	uniformData.currentValue = m_currentStorage;

	Wolf::BoundingSphere boundingSphere = m_entity->getBoundingSphere();
	uniformData.minY = boundingSphere.getCenter().y - boundingSphere.getRadius();
	uniformData.maxY = boundingSphere.getCenter().y + boundingSphere.getRadius();

	m_uniformBuffer->transferCPUMemory(&uniformData, sizeof(UniformData), 0);
}

void GasCylinderComponent::alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList)
{
	for (DrawManager::DrawMeshInfo& drawMeshInfo : renderMeshList)
	{
		Wolf::RenderMeshList::MeshToRender& meshToRender = drawMeshInfo.meshToRender;

		Wolf::ResourceUniqueOwner<Wolf::PipelineSet>& replacePipelineSet = m_pipelineSetMapping[meshToRender.pipelineSet->getPipelineHash(0)]; // TODO: use all pipelines hashes
		if (!replacePipelineSet)
		{
			replacePipelineSet.reset(new Wolf::PipelineSet);

			std::vector<const Wolf::PipelineSet::PipelineInfo*> allPipelinesInfo = meshToRender.pipelineSet->retrieveAllPipelinesInfo();
			for (uint32_t i = 0; i < allPipelinesInfo.size(); ++i)
			{
				const Wolf::PipelineSet::PipelineInfo* pipelineInfo = allPipelinesInfo[i];

				Wolf::PipelineSet::PipelineInfo newPipelineInfo = *pipelineInfo;

				if (i == CommonPipelineIndices::PIPELINE_IDX_FORWARD)
				{
					for (Wolf::PipelineSet::PipelineInfo::ShaderInfo& shaderInfo : newPipelineInfo.shaderInfos)
					{
						if (shaderInfo.stage == Wolf::ShaderStageFlagBits::FRAGMENT)
						{
							shaderInfo.shaderFilename = "Shaders/gasCylinder/shader.frag";
						}
					}
				}
				replacePipelineSet->addPipeline(newPipelineInfo);
			}
		}

		meshToRender.pipelineSet = replacePipelineSet.createConstNonOwnerResource();
		meshToRender.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(m_descriptorSet.createConstNonOwnerResource(), m_descriptorSetLayout.createConstNonOwnerResource(), 
			DescriptorSetSlots::DESCRIPTOR_SET_SLOT_COUNT);
	}
}

void GasCylinderComponent::addShootRequest(const Wolf::Timer& globalTimer)
{
	static constexpr float COST_PER_MS = 1.0f / 7.0f;
	float currentTotalStorage = static_cast<float>(static_cast<float>(m_maxStorage) * m_currentStorage);
	currentTotalStorage -= static_cast<float>(globalTimer.getElapsedTimeSinceLastUpdateInMs()) * COST_PER_MS;
	if (currentTotalStorage < 0)
		currentTotalStorage = 0;
	m_currentStorage = static_cast<float>(currentTotalStorage) / static_cast<float>(m_maxStorage);
}

void GasCylinderComponent::setLinkPositions(const glm::vec3& topPos, const glm::vec3& botPos)
{
	glm::vec3 t, r;

	computeTransformFromTwoPoints(botPos, topPos, t, r);
	m_entity->setPosition(t);
	m_entity->setRotation(r);

	notifySubscribers();
}

void GasCylinderComponent::onPlayerRelease()
{
	Wolf::Physics::PhysicsManager::RayCastResult rayCastResult = m_physicsManager->rayCastClosestHit(m_entity->getPosition(), m_entity->getPosition() - glm::vec3(0.0f, 10.0f, 0.0f));
	if (!rayCastResult.collision)
	{
		Wolf::Debug::sendError("Gas cylinder didn't find surface to drop on");
	}
	else
	{
		m_entity->setPosition(rayCastResult.hitPoint + m_entity->getBoundingSphere().getRadius() / 2.0f);
		m_entity->setRotation(glm::vec3(0.0f));
	}
}

void GasCylinderComponent::onMaterialAdded()
{
	m_contaminationMaterials.back().setGetEntityFromLoadingPathCallback(m_getEntityFromLoadingPathCallback);
	m_contaminationMaterials.back().subscribe(this, [this](Flags) { onMaterialChanged(); });
	m_requestReloadCallback(this);
}

void GasCylinderComponent::onMaterialChanged()
{
	m_color = glm::vec3(0.0f);
	m_cleanFlags = 0;

	for (uint32_t i = 0; i < m_contaminationMaterials.size(); ++i)
	{
		m_color += m_contaminationMaterials[i].getColor();
		m_cleanFlags |= (1 << m_contaminationMaterials[i].getIdxInContaminationEmitter());
	}

	m_color /= static_cast<float>(m_contaminationMaterials.size());
}
