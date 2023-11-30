#include "ObjectModel.h"

#include <AABB.h>
#include <DescriptorSetGenerator.h>
#include <Timer.h>
#include <Pipeline.h>

#include "CommonDescriptorLayouts.h"

using namespace Wolf;

ObjectModel::ObjectModel(const glm::mat4& transform, const std::string& filename, const std::string& mtlFolder, bool loadMaterials, uint32_t materialIdOffset) : EditorModelInterface(transform)
{
	Timer timer(filename + " loading");

	m_defaultPipelineSet.reset(new LazyInitSharedResource<PipelineSet, ObjectModel>([this](std::unique_ptr<PipelineSet>& pipelineSet)
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
			pipelineInfo.descriptorSetLayouts = { m_modelDescriptorSetLayout->getResource()->getDescriptorSetLayout(), CommonDescriptorLayouts::g_commonDescriptorSetLayout };
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

	ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = filename;
	modelLoadingInfo.mtlFolder = mtlFolder;
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.loadMaterials = loadMaterials;
	modelLoadingInfo.materialIdOffset = materialIdOffset;
	ModelLoader::loadObject(m_modelData, modelLoadingInfo);

	m_descriptorSet.reset(new DescriptorSet(m_modelDescriptorSetLayout->getResource()->getDescriptorSetLayout(), UpdateRate::NEVER));

	DescriptorSetGenerator descriptorSetGenerator(m_modelDescriptorSetLayoutGenerator->getResource()->getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *m_matricesUniformBuffer);
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	const uint32_t lastBackslashCharPos = filename.find_last_of('\\') + 1;
	m_nameParam = filename.substr(lastBackslashCharPos, filename.size() - lastBackslashCharPos);
	m_filename = filename;
}

void ObjectModel::addMeshesToRenderList(RenderMeshList& renderMeshList) const
{
	RenderMeshList::MeshToRenderInfo meshToRenderInfo(m_modelData.mesh.get(), m_defaultPipelineSet->getResource());
	meshToRenderInfo.descriptorSets.push_back({ m_descriptorSet.get(), 1 });

	renderMeshList.addMeshToRender(meshToRenderInfo);
}

const AABB& ObjectModel::getAABB() const
{
	return m_modelData.mesh->getAABB() * m_transform;
}
