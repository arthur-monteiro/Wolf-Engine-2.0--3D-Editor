#include "StaticModel.h"

#include <AABB.h>
#include <DescriptorSetGenerator.h>
#include <Timer.h>
#include <Pipeline.h>
#include <ProfilerCommon.h>

#include "CommonLayouts.h"
#include "DrawManager.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"

using namespace Wolf;

StaticModel::StaticModel(const glm::mat4& transform, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ResourceManager>& resourceManager)
: EditorModelInterface(transform), m_materialsGPUManager(materialsGPUManager), m_resourceManager(resourceManager)
{
	m_defaultPipelineSet.reset(new LazyInitSharedResource<PipelineSet, StaticModel>([this](Wolf::ResourceUniqueOwner<PipelineSet>& pipelineSet)
		{
			pipelineSet.reset(new PipelineSet);

			PipelineSet::PipelineInfo pipelineInfo;

			/* Pre Depth */
			pipelineInfo.shaderInfos.resize(1);
			pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/defaultPipeline/shader.vert";
			pipelineInfo.shaderInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

			// IA
			Vertex3D::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);
			InstanceData::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 1);

			pipelineInfo.vertexInputBindingDescriptions.resize(2);
			Vertex3D::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);
			InstanceData::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[1], 1);

			// Resources
			pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

			// Color Blend
			pipelineInfo.blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH);

			/* Shadow maps */
			pipelineInfo.dynamicStates.clear();
			pipelineInfo.depthBiasConstantFactor = 4.0f;
			pipelineInfo.depthBiasSlopeFactor = 2.5f;
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
			pipelineInfo.depthBiasConstantFactor = 0.0f;
			pipelineInfo.depthBiasSlopeFactor = 0.0f;

			/* Forward */
			pipelineInfo.shaderInfos.resize(2);
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultPipeline/shader.frag";
			pipelineInfo.shaderInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

			// Resources
			pipelineInfo.bindlessDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_BINDLESS;
			pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
			pipelineInfo.customMask = AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO;

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
		}));
}

void StaticModel::loadParams(JSONReader& jsonReader)
{
	std::vector<EditorParamInterface*> params = { &m_loadingPathParam };
	::loadParams(jsonReader, ID, params);
	::loadParams(jsonReader, ID, m_modelParams);
}

void StaticModel::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
	PROFILE_FUNCTION

	EditorModelInterface::updateBeforeFrame(globalTimer);
}

void StaticModel::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
	PROFILE_FUNCTION

	if (!m_resourceManager->isModelLoaded(m_meshResourceId))
		return;

	Wolf::RenderMeshList::MeshToRender meshToRenderInfo(m_resourceManager->getModelData(m_meshResourceId)->mesh.createNonOwnerResource(), m_defaultPipelineSet->getResource().createConstNonOwnerResource());
	
	outList.push_back({ meshToRenderInfo, { m_transform }});
}

void StaticModel::activateParams()
{
	EditorModelInterface::activateParams();

	m_loadingPathParam.activate();
}

void StaticModel::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	EditorModelInterface::addParamsToJSON(outJSON, tabCount);

	std::vector<EditorParamInterface*> params = { &m_loadingPathParam };
	::addParamsToJSON(outJSON, params, false, tabCount);
}

AABB StaticModel::getAABB() const
{
	if (m_resourceManager->isModelLoaded(m_meshResourceId))
		return m_resourceManager->getModelData(m_meshResourceId)->mesh->getAABB() * m_transform;

	return AABB();
}

void StaticModel::requestModelLoading()
{
	if (!std::string(m_loadingPathParam).empty())
	{
		m_meshResourceId = m_resourceManager->addModel(std::string(m_loadingPathParam));
		notifySubscribers();
	}
}
