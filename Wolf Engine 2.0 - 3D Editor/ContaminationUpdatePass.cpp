#include "ContaminationUpdatePass.h"

#include <ProfilerCommon.h>

#include "ContaminationEmitter.h"
#include "DebugMarker.h"

void ContaminationUpdatePass::initializeResources(const Wolf::InitializationContext& context)
{
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false)); // Can't run on compute queue as contamination image ids is on graphic queue
	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

	m_computeShaderParser.reset(new Wolf::ShaderParser("Shaders/contamination/update.comp"));
	m_shootRequestBuffer.reset(new Wolf::UniformBuffer(sizeof(ShootRequestGPUInfo)));

	m_descriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::COMPUTE, 0); // Contamination Ids
	m_descriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 1); // Contamination info
	m_descriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 2); // Shoot requests
	m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	createPipeline();
}

void ContaminationUpdatePass::resize(const Wolf::InitializationContext& context)
{
	// Nothing to do
}

void ContaminationUpdatePass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	if (m_contaminationEmitters.empty())
	{
		m_wasEnabledThisFrame = false;
		return;
	}

	static constexpr float SHOOT_DURATION_MS = 1000.0f;

	// Remove old requests
	for (int32_t i = static_cast<int32_t>(m_shootRequests.size()) - 1; i >= 0; --i)
	{
		if (static_cast<uint32_t>(context.globalTimer->getCurrentCachedMillisecondsDuration()) - m_shootRequests[i].startTimerInMs > static_cast<uint32_t>(SHOOT_DURATION_MS))
		{
			m_shootRequests.erase(m_shootRequests.begin() + i);
		}
	}

	// Add new request to requests
	m_shootRequestsLock.lock();
	for (const ShootRequest& shootRequest : m_newShootRequests)
	{
		m_shootRequests.emplace_back(shootRequest);
	}
	m_newShootRequests.clear();
	m_shootRequestsLock.unlock();

	if (m_shootRequests.size() > ShootRequestGPUInfo::MAX_SHOOT_REQUEST)
	{
		Wolf::Debug::sendCriticalError("Too many shoot requests");
	}

	// Add requests to the GPU
	ShootRequestGPUInfo shootRequestGPUInfo{};
	for (const ShootRequest& shootRequest : m_shootRequests)
	{
		float shootRequestLength = shootRequest.length * (static_cast<float>(static_cast<uint32_t>(context.globalTimer->getCurrentCachedMillisecondsDuration()) - shootRequest.startTimerInMs) / SHOOT_DURATION_MS);

		shootRequestGPUInfo.startPointAndLength[shootRequestGPUInfo.shootRequestCount] = glm::vec4(shootRequest.startPoint, shootRequestLength);
		shootRequestGPUInfo.directionAndRadius[shootRequestGPUInfo.shootRequestCount] = glm::vec4(shootRequest.direction, shootRequest.radius);

		float offset = shootRequestLength - shootRequest.smokeParticleSize;
		shootRequestGPUInfo.startPointOffsetAndMaterialsCleanedFlags[shootRequestGPUInfo.shootRequestCount] = glm::vec4(offset, std::bit_cast<float>(shootRequest.materialsCleanedFlags), -1.0f, -1.0f);
		shootRequestGPUInfo.shootRequestCount++;
	}
	m_shootRequestBuffer->transferCPUMemory(&shootRequestGPUInfo, sizeof(shootRequestGPUInfo));

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::computePassDebugColor, "Contamination update pass");

	m_contaminationEmitters[0].getContaminationEmitter()->getContaminationIdsImage()->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

	m_commandBuffer->bindPipeline(m_computePipeline.createConstNonOwnerResource());
	m_commandBuffer->bindDescriptorSet(m_contaminationEmitters[0].getUpdateDescriptorSet().createConstNonOwnerResource(), 0, *m_computePipeline);

	constexpr Wolf::Extent3D dispatchGroups = { 8, 8, 8 };
	constexpr uint32_t groupSizeX = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.width;
	constexpr uint32_t groupSizeY = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.height;
	constexpr uint32_t groupSizeZ = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.depth;
	m_commandBuffer->dispatch(groupSizeX, groupSizeY, groupSizeZ);

	m_contaminationEmitters[0].getContaminationEmitter()->getContaminationIdsImage()->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, VK_IMAGE_LAYOUT_GENERAL });

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();

	m_wasEnabledThisFrame = true;
}

void ContaminationUpdatePass::submit(const Wolf::SubmitContext& context)
{
	if (m_wasEnabledThisFrame)
	{
		const std::vector<const Wolf::Semaphore*> waitSemaphores{  };
		const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };
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
	m_newShootRequests.swap(shootRequests);
	m_shootRequestsLock.unlock();
}

void ContaminationUpdatePass::createPipeline()
{
	Wolf::ShaderCreateInfo computeShaderInfo;
	computeShaderInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;
	m_computeShaderParser->readCompiledShader(computeShaderInfo.shaderCode);

	std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts = { m_descriptorSetLayout.createConstNonOwnerResource() };

	m_computePipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderInfo, descriptorSetLayouts));
}

ContaminationUpdatePass::PerEmitterInfo::PerEmitterInfo(ContaminationEmitter* contaminationEmitter, const Wolf::DescriptorSetLayoutGenerator& descriptorSetLayoutGenerator, const Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& descriptorSetLayout,
	const Wolf::ResourceUniqueOwner<Wolf::UniformBuffer>& shootRequestsBuffer)
{
	m_componentPtr = contaminationEmitter;

	Wolf::DescriptorSetGenerator descriptorSetGenerator(descriptorSetLayoutGenerator.getDescriptorLayouts());
	descriptorSetGenerator.setImage(0, { VK_IMAGE_LAYOUT_GENERAL, contaminationEmitter->getContaminationIdsImage()->getDefaultImageView() });
	descriptorSetGenerator.setBuffer(1, *contaminationEmitter->getContaminationInfoBuffer());
	descriptorSetGenerator.setUniformBuffer(2, *shootRequestsBuffer);

	m_updateDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*descriptorSetLayout));
	m_updateDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}
