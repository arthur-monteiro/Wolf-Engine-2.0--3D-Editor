#include "GasCylinderComponent.h"

#include <glm/gtx/matrix_decompose.hpp>

#include "CommonLayouts.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "MathsUtilsEditor.h"
#include "ModelLoader.h"
#include "StaticModel.h"

GasCylinderComponent::GasCylinderComponent()
{
	m_descriptorSetLayoutGenerator.addUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());

	m_uniformBuffer.reset(Wolf::Buffer::createBuffer(sizeof(UniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	descriptorSetGenerator.setBuffer(0, *m_uniformBuffer);

	m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void GasCylinderComponent::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_editorParams);
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

void GasCylinderComponent::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
	UniformData uniformData{};

	uniformData.color = static_cast<glm::vec3>(m_color);
	uniformData.currentValue = m_currentStorage;

	Wolf::AABB aabb = m_entity->getAABB() * m_forcedTransform;
	uniformData.minY = aabb.getMin().y;
	uniformData.maxY = aabb.getMax().y;

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
						if (shaderInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
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

		if (m_usedForcedTransform)
		{
			glm::vec3 scale;
			glm::quat quatRotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(drawMeshInfo.instanceData.transform, scale, quatRotation, translation, skew, perspective);

			drawMeshInfo.instanceData.transform = glm::scale(m_forcedTransform, scale);
		}
	}
}

void GasCylinderComponent::addShootRequest(const Wolf::Timer& globalTimer)
{
	static constexpr uint32_t COST_PER_FRAME = 1; // TODO: make it time based instead of per frame
	int32_t currentTotalStorage = static_cast<int32_t>(static_cast<float>(m_maxStorage) * m_currentStorage);
	currentTotalStorage -= COST_PER_FRAME;
	if (currentTotalStorage < 0)
		currentTotalStorage = 0;
	m_currentStorage = static_cast<float>(currentTotalStorage) / static_cast<float>(m_maxStorage);
}

void GasCylinderComponent::setLinkPositions(const glm::vec3& topPos, const glm::vec3& botPos)
{
	computeTransformFromTwoPoints(botPos, topPos, m_forcedTransform);
	m_usedForcedTransform = true;

	notifySubscribers();
}
