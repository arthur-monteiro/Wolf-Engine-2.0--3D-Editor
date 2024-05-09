#include "ContaminationReceiver.h"

#include <fstream>
#include <utility>

#include "ContaminationEmitter.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "PipelineSet.h"

ContaminationReceiver::ContaminationReceiver(std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback) : m_getEntityFromLoadingPathCallback(std::move(getEntityFromLoadingPathCallback))
{
}

void ContaminationReceiver::loadParams(Wolf::JSONReader& jsonReader)
{
	std::vector<EditorParamInterface*> params = { &m_contaminationEmitterParam };
	::loadParams(jsonReader, ID, params);
}

void ContaminationReceiver::activateParams()
{
	m_contaminationEmitterParam.activate();
}

void ContaminationReceiver::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	std::vector<EditorParamInterface*> params = { &m_contaminationEmitterParam };
	::addParamsToJSON(outJSON, params, false, tabCount);
}

void ContaminationReceiver::alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList)
{
	if (m_contaminationEmitterEntity)
	{
		if (const Wolf::ResourceNonOwner<ContaminationEmitter> contaminationEmitterComponent = (*m_contaminationEmitterEntity)->getComponent<ContaminationEmitter>())
		{
			for (Wolf::RenderMeshList::MeshToRenderInfo& meshToRender : renderMeshList)
			{
				std::unique_ptr<Wolf::PipelineSet>& replacePipelineSet = m_pipelineSetMapping[meshToRender.pipelineSet];
				if (!replacePipelineSet)
				{
					replacePipelineSet.reset(new Wolf::PipelineSet);
					for (const Wolf::PipelineSet::PipelineInfo* pipelineInfo : meshToRender.pipelineSet->retrieveAllPipelinesInfo())
					{
						Wolf::PipelineSet::PipelineInfo newPipelineInfo = *pipelineInfo;
						if (newPipelineInfo.bindlessDescriptorSlot != static_cast<uint32_t>(-1))
						{
							newPipelineInfo.descriptorSetLayouts.emplace_back(contaminationEmitterComponent->getDescriptorSetLayout().createConstNonOwnerResource(), 4);
						}
						for (Wolf::PipelineSet::PipelineInfo::ShaderInfo& shaderInfo : newPipelineInfo.shaderInfos)
						{
							if (shaderInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
							{
								shaderInfo.materialFetchProcedure.codeString = "";

								std::ifstream inFile("Shaders/materialFetchProcedures/contaminationReceiver.glsl");
								std::string line;
								while (std::getline(inFile, line))
								{
									const std::string& descriptorSlotToken = "£CONTAMINATION_DESCRIPTOR_SLOT";
									size_t descriptorSlotTokenPos = line.find(descriptorSlotToken);
									while (descriptorSlotTokenPos != std::string::npos)
									{
										line.replace(descriptorSlotTokenPos, descriptorSlotToken.length(), std::to_string(4)); // TODO: replace this '4' with an available slot
										descriptorSlotTokenPos = line.find(descriptorSlotToken);
									}
									shaderInfo.materialFetchProcedure.codeString += line + '\n';
								}
							}
						}
						replacePipelineSet->addPipeline(newPipelineInfo);
					}
				}

				meshToRender.pipelineSet = replacePipelineSet.get();
				meshToRender.descriptorSets.push_back({ contaminationEmitterComponent->getDescriptorSet().createConstNonOwnerResource(), 4 });
			}
		}
	}
}

void ContaminationReceiver::onContaminationEmitterChanged()
{
	m_contaminationEmitterEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_contaminationEmitterParam)));
}
