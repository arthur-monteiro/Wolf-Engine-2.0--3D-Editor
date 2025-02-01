#include "ContaminationUpdatePass.h"

#include <ProfilerCommon.h>

#include "ContaminationEmitter.h"
#include "DebugMarker.h"

using namespace Wolf;

void ContaminationUpdatePass::initializeResources(const InitializationContext& context)
{
	m_commandBuffer.reset(CommandBuffer::createCommandBuffer(QueueType::GRAPHIC, false)); // Can't run on compute queue as contamination image ids is on graphic queue
	m_semaphore.reset(Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

	m_computeShaderParser.reset(new ShaderParser("Shaders/contamination/update.comp"));
	m_shootRequestBuffer.reset(Buffer::createBuffer(sizeof(ShootRequestGPUInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

	m_descriptorSetLayoutGenerator.addStorageImage(VK_SHADER_STAGE_COMPUTE_BIT, 0); // Contamination Ids
	m_descriptorSetLayoutGenerator.addStorageBuffer(VK_SHADER_STAGE_COMPUTE_BIT, 1); // Contamination info
	m_descriptorSetLayoutGenerator.addUniformBuffer(VK_SHADER_STAGE_COMPUTE_BIT, 2); // Shoot requests
	m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	createPipeline();
}

void ContaminationUpdatePass::resize(const InitializationContext& context)
{
	// Nothing to do
}

void ContaminationUpdatePass::record(const RecordContext& context)
{
	PROFILE_FUNCTION

	if (m_contaminationEmitters.empty())
	{
		m_wasEnabledThisFrame = false;
		return;
	}

	ShootRequestGPUInfo shootRequestGPUInfo{};
	m_shootRequestsLock.lock();
	for (const ShootRequest& shootRequest : m_shootRequests)
	{
		shootRequestGPUInfo.startPointAndLength[shootRequestGPUInfo.shootRequestCount] = glm::vec4(shootRequest.startPoint, shootRequest.length);
		shootRequestGPUInfo.directionAndRadius[shootRequestGPUInfo.shootRequestCount] = glm::vec4(shootRequest.direction, shootRequest.radius);
		shootRequestGPUInfo.shootRequestCount++;
	}
	m_shootRequests.clear();
	m_shootRequestsLock.unlock();
	m_shootRequestBuffer->transferCPUMemory(&shootRequestGPUInfo, sizeof(shootRequestGPUInfo));

	m_commandBuffer->beginCommandBuffer();

	DebugMarker::beginRegion(m_commandBuffer.get(), DebugMarker::computePassDebugColor, "Contamination update pass");

	m_contaminationEmitters[0].getContaminationEmitter()->getContaminationIdsImage()->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

	m_commandBuffer->bindPipeline(m_computePipeline.createConstNonOwnerResource());
	m_commandBuffer->bindDescriptorSet(m_contaminationEmitters[0].getUpdateDescriptorSet().createConstNonOwnerResource(), 0, *m_computePipeline);

	constexpr VkExtent3D dispatchGroups = { 8, 8, 8 };
	constexpr uint32_t groupSizeX = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.width;
	constexpr uint32_t groupSizeY = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.height;
	constexpr uint32_t groupSizeZ = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.depth;
	m_commandBuffer->dispatch(groupSizeX, groupSizeY, groupSizeZ);

	m_contaminationEmitters[0].getContaminationEmitter()->getContaminationIdsImage()->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, VK_IMAGE_LAYOUT_GENERAL });

	DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();

	m_wasEnabledThisFrame = true;
}

void ContaminationUpdatePass::submit(const SubmitContext& context)
{
	if (m_wasEnabledThisFrame)
	{
		const std::vector<const Semaphore*> waitSemaphores{  };
		const std::vector<const Semaphore*> signalSemaphores{ m_semaphore.get() };
		m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);

		if (m_computeShaderParser->compileIfFileHasBeenModified())
		{
			context.graphicAPIManager->waitIdle();
			createPipeline();
		}
	}
}

void ContaminationUpdatePass::registerEmitter(ContaminationEmitter* componentEmitter)
{
	m_contaminationEmitterLock.lock();
	
	m_contaminationEmitters.emplace_back(componentEmitter, m_descriptorSetLayoutGenerator, m_descriptorSetLayout, m_shootRequestBuffer);

	m_contaminationEmitterLock.unlock();
}

void ContaminationUpdatePass::unregisterEmitter(const ContaminationEmitter* componentEmitter)
{
	m_contaminationEmitterLock.lock();
	for (uint32_t i = 0; i < m_contaminationEmitters.size(); ++i)
	{
		if (m_contaminationEmitters[i].isSame(componentEmitter))
		{
			m_contaminationEmitters.erase(m_contaminationEmitters.begin() + i);
			break;
		}
	}
	m_contaminationEmitterLock.unlock();
}

void ContaminationUpdatePass::swapShootRequests(std::vector<ShootRequest>& shootRequests)
{
	m_shootRequestsLock.lock();
	m_shootRequests.swap(shootRequests);
	m_shootRequestsLock.unlock();
}

void ContaminationUpdatePass::createPipeline()
{
	ShaderCreateInfo computeShaderInfo;
	computeShaderInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	m_computeShaderParser->readCompiledShader(computeShaderInfo.shaderCode);

	std::vector<ResourceReference<const DescriptorSetLayout>> descriptorSetLayouts = { m_descriptorSetLayout.createConstNonOwnerResource() };

	m_computePipeline.reset(Pipeline::createComputePipeline(computeShaderInfo, descriptorSetLayouts));
}

ContaminationUpdatePass::PerEmitterInfo::PerEmitterInfo(ContaminationEmitter* contaminationEmitter, const Wolf::DescriptorSetLayoutGenerator& descriptorSetLayoutGenerator, const Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& descriptorSetLayout,
	const Wolf::ResourceUniqueOwner<Wolf::Buffer>& shootRequestsBuffer)
{
	m_componentPtr = contaminationEmitter;

	Wolf::DescriptorSetGenerator descriptorSetGenerator(descriptorSetLayoutGenerator.getDescriptorLayouts());
	descriptorSetGenerator.setImage(0, { VK_IMAGE_LAYOUT_GENERAL, contaminationEmitter->getContaminationIdsImage()->getDefaultImageView() });
	descriptorSetGenerator.setBuffer(1, *contaminationEmitter->getContaminationInfoBuffer());
	descriptorSetGenerator.setBuffer(2, *shootRequestsBuffer);

	m_updateDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*descriptorSetLayout));
	m_updateDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}
