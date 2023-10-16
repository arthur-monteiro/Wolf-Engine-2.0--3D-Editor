#include "ObjectModel.h"

#include <AABB.h>
#include <DescriptorSetGenerator.h>
#include <Timer.h>

#include "DefaultPipelineInfos.h"

using namespace Wolf;

ObjectModel::ObjectModel(const glm::mat4& transform, bool buildAccelerationStructures, const std::string& filename,
	const std::string& mtlFolder, bool loadMaterials, uint32_t materialIdOffset) : ModelInterface(transform)
{
	Timer timer(filename + " loading");

	ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = filename;
	modelLoadingInfo.mtlFolder = mtlFolder;
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.loadMaterials = loadMaterials;
	modelLoadingInfo.materialIdOffset = materialIdOffset;
	ObjLoader::loadObject(m_modelData, modelLoadingInfo);

	m_descriptorSet.reset(new DescriptorSet(DefaultPipeline::g_descriptorSetLayout, UpdateRate::NEVER));

	DescriptorSetGenerator descriptorSetGenerator(DefaultPipeline::g_descriptorSetLayoutGenerator.getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *m_matricesUniformBuffer);
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	uint32_t lastBackslashCharPos = filename.find_last_of('\\') + 1;
	m_name = filename.substr(lastBackslashCharPos, filename.size() - lastBackslashCharPos);
	m_filename = filename;
}

void ObjectModel::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const
{
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, m_descriptorSet->getDescriptorSet(), 0, nullptr);
	m_modelData.mesh->draw(commandBuffer);
}

const AABB& ObjectModel::getAABB() const
{
	return m_modelData.mesh->getAABB() * m_transform;
}