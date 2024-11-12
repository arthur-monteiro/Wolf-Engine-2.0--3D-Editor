#include "ContaminationUpdatePass.h"

#include <ProfilerCommon.h>

#include "ContaminationEmitter.h"
#include "DebugMarker.h"

using namespace Wolf;

void ContaminationUpdatePass::initializeResources(const InitializationContext& context)
{
	m_commandBuffer.reset(CommandBuffer::createCommandBuffer(QueueType::COMPUTE, false));
	m_semaphore.reset(Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

	m_computeShaderParser.reset(new ShaderParser("Shaders/contamination/update.comp"));

	createPipeline();
}

void ContaminationUpdatePass::resize(const InitializationContext& context)
{
	// Nothing to do
}

void ContaminationUpdatePass::record(const RecordContext& context)
{
	PROFILE_FUNCTION

	for (const ShootRequest& shootRequest : m_shootRequests)
	{
		// TODO: add shoot requests to a buffer
	}
	m_shootRequests.clear();

	m_commandBuffer->beginCommandBuffer();

	DebugMarker::beginRegion(m_commandBuffer.get(), DebugMarker::computePassDebugColor, "Contamination update pass");

	m_commandBuffer->bindPipeline(m_computePipeline.createConstNonOwnerResource());

	constexpr VkExtent3D dispatchGroups = { 8, 8, 8 };
	constexpr uint32_t groupSizeX = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.width;
	constexpr uint32_t groupSizeY = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.height;
	constexpr uint32_t groupSizeZ = ContaminationEmitter::CONTAMINATION_IDS_IMAGE_SIZE / dispatchGroups.depth;
	m_commandBuffer->dispatch(groupSizeX, groupSizeY, groupSizeZ);

	DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void ContaminationUpdatePass::submit(const SubmitContext& context)
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

void ContaminationUpdatePass::registerEmitter(ContaminationEmitter* componentEmitter)
{
	m_contaminationEmitterLock.lock();

	m_contaminationEmitters.push_back(componentEmitter);
	// TODO: add 3D texture to a descriptor set

	m_contaminationEmitterLock.unlock();
}

void ContaminationUpdatePass::unregisterEmitter(ContaminationEmitter* componentEmitter)
{
	m_contaminationEmitterLock.lock();
	std::erase(m_contaminationEmitters, componentEmitter);
	m_contaminationEmitterLock.unlock();
}

void ContaminationUpdatePass::swapShootRequests(std::vector<ShootRequest>& shootRequests)
{
	m_shootRequests.swap(shootRequests);
}

void ContaminationUpdatePass::createPipeline()
{
	ShaderCreateInfo computeShaderInfo;
	computeShaderInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	m_computeShaderParser->readCompiledShader(computeShaderInfo.shaderCode);

	std::vector<ResourceReference<const DescriptorSetLayout>> descriptorSetLayouts = { };

	m_computePipeline.reset(Pipeline::createComputePipeline(computeShaderInfo, descriptorSetLayouts));
}
