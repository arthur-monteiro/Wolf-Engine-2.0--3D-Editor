#include "StaticModel.h"

#include <AABB.h>
#include <DescriptorSetGenerator.h>
#include <Timer.h>
#include <Pipeline.h>

#include "CommonDescriptorLayouts.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"

using namespace Wolf;

StaticModel::StaticModel(const glm::mat4& transform, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager)
: EditorModelInterface(transform), m_materialsGPUManager(materialsGPUManager)
{
	m_defaultPipelineSet.reset(new LazyInitSharedResource<PipelineSet, StaticModel>([this](std::unique_ptr<PipelineSet>& pipelineSet)
		{
			pipelineSet.reset(new PipelineSet);

			PipelineSet::PipelineInfo pipelineInfo;

			pipelineInfo.shaderInfos.resize(2);
			pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/defaultPipeline/shader.vert";
			pipelineInfo.shaderInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultPipeline/shader.frag";
			pipelineInfo.shaderInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

			// IA
			Vertex3D::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

			pipelineInfo.vertexInputBindingDescriptions.resize(1);
			Vertex3D::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

			// Resources
			pipelineInfo.descriptorSetLayouts = { { m_modelDescriptorSetLayout->getResource()->getDescriptorSetLayout(), 1 }, { CommonDescriptorLayouts::g_commonDescriptorSetLayout, 2 } };
			pipelineInfo.bindlessDescriptorSlot = 0;
			pipelineInfo.cameraDescriptorSlot = 3;

			// Color Blend
			pipelineInfo.blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

			const uint32_t pipelineIdx = pipelineSet->addPipeline(pipelineInfo);
			if (pipelineIdx != 0)
				Debug::sendError("Unexpected pipeline idx");
		}));
}

void StaticModel::loadParams(JSONReader& jsonReader)
{
	std::vector<EditorParamInterface*> params = { &m_loadingPathParam };
	::loadParams(jsonReader, ID, params);
	::loadParams(jsonReader, ID, m_modelParams);
}

void StaticModel::updateBeforeFrame()
{
	EditorModelInterface::updateBeforeFrame();

	if (m_modelLoadingRequested)
	{
		loadModel();
		m_modelLoadingRequested = false;
	}
}

void StaticModel::getMeshesToRender(std::vector<RenderMeshList::MeshToRenderInfo>& outList)
{
	if (!m_modelData.mesh)
		return;

	RenderMeshList::MeshToRenderInfo meshToRenderInfo(m_modelData.mesh.createNonOwnerResource(), m_defaultPipelineSet->getResource());
	meshToRenderInfo.descriptorSets.push_back({ m_descriptorSet.createConstNonOwnerResource(), 1 });

	outList.push_back(meshToRenderInfo);
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
	if (m_modelData.mesh)
		return m_modelData.mesh->getAABB() * m_transform;

	return AABB();
}

void StaticModel::loadModel()
{
	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPathParam);

	Timer timer(std::string(m_loadingPathParam) + " loading");

	ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = fullFilePath;
	modelLoadingInfo.mtlFolder = fullFilePath.substr(0, fullFilePath.find_last_of('\\'));
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.materialLayout = MaterialLoader::InputMaterialLayout::EACH_TEXTURE_A_FILE;
	modelLoadingInfo.materialIdOffset = m_materialsGPUManager->getCurrentMaterialCount();
	ModelLoader::loadObject(m_modelData, modelLoadingInfo);

	m_descriptorSet.reset(new DescriptorSet(m_modelDescriptorSetLayout->getResource()->getDescriptorSetLayout(), UpdateRate::NEVER));

	DescriptorSetGenerator descriptorSetGenerator(m_modelDescriptorSetLayoutGenerator->getResource()->getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *m_matricesUniformBuffer);
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	std::vector<DescriptorSetGenerator::ImageDescription> imageDescriptions(m_modelData.images.size());
	for (uint32_t i = 0; i < m_modelData.images.size(); ++i)
	{
		imageDescriptions[i].imageView = m_modelData.images[i]->getDefaultImageView();
		imageDescriptions[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	m_materialsGPUManager->addNewMaterials(imageDescriptions);
}

void StaticModel::requestModelLoading()
{
	if (!std::string(m_loadingPathParam).empty()) 
		m_modelLoadingRequested = true;
}
