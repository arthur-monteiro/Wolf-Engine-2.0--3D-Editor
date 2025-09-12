#include "ContaminationReceiver.h"

#include <fstream>
#include <utility>

#include "CommonLayouts.h"
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

void ContaminationReceiver::alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList)
{
	if (m_contaminationEmitterEntity)
	{
		if (const Wolf::ResourceNonOwner<ContaminationEmitter> contaminationEmitterComponent = (*m_contaminationEmitterEntity)->getComponent<ContaminationEmitter>())
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
									shaderInfo.materialFetchProcedure.codeString = "";

									std::ifstream inFile("Shaders/materialFetchProcedures/contaminationReceiver.glsl");
									std::string line;
									while (std::getline(inFile, line))
									{
										const std::string& descriptorSlotToken = "@CONTAMINATION_DESCRIPTOR_SLOT";
										size_t descriptorSlotTokenPos = line.find(descriptorSlotToken);
										while (descriptorSlotTokenPos != std::string::npos)
										{
											line.replace(descriptorSlotTokenPos, descriptorSlotToken.length(), std::to_string(DescriptorSetSlots::DESCRIPTOR_SET_SLOT_COUNT));
											descriptorSlotTokenPos = line.find(descriptorSlotToken);
										}
										shaderInfo.materialFetchProcedure.codeString += line + '\n';
									}
								}
							}
						}
						replacePipelineSet->addPipeline(newPipelineInfo);
					}
				}

				meshToRender.pipelineSet = replacePipelineSet.createConstNonOwnerResource();
				meshToRender.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(contaminationEmitterComponent->getDescriptorSet().createConstNonOwnerResource(), 
					contaminationEmitterComponent->getDescriptorSetLayout().createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_COUNT);
			}
		}
	}
}

void ContaminationReceiver::onContaminationEmitterChanged()
{
	m_contaminationEmitterEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_contaminationEmitterParam)));
}
