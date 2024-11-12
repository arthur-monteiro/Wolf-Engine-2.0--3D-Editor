#include "ParticleUpdatePass.h"

#include <random>

#include <ProfilerCommon.h>

#include "DebugMarker.h"
#include "Pipeline.h"

void ParticleUpdatePass::initializeResources(const Wolf::InitializationContext& context)
{
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::COMPUTE, false));
	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

	m_computeShaderParser.reset(new Wolf::ShaderParser("Shaders/particles/update.comp"));

	m_particlesBuffer.reset(Wolf::Buffer::createBuffer(MAX_TOTAL_PARTICLES * sizeof(ParticleInfoGPU), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	m_emitterDrawInfoBuffer.reset(Wolf::Buffer::createBuffer(MAX_EMITTER_COUNT * sizeof(EmitterDrawInfo), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

	m_uniformBuffer.reset(Wolf::Buffer::createBuffer(sizeof(UniformBufferData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	createNoiseBuffer();

	m_descriptorSetLayoutGenerator.addUniformBuffer(VK_SHADER_STAGE_COMPUTE_BIT, 0);
	m_descriptorSetLayoutGenerator.addStorageBuffer(VK_SHADER_STAGE_COMPUTE_BIT, 1);
	m_descriptorSetLayoutGenerator.addStorageBuffer(VK_SHADER_STAGE_COMPUTE_BIT, 2);
	m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *m_uniformBuffer);
	descriptorSetGenerator.setBuffer(1, *m_particlesBuffer);
	descriptorSetGenerator.setBuffer(2, *m_noiseBuffer);

	m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	createPipeline();
}

void ParticleUpdatePass::resize(const Wolf::InitializationContext& context)
{
	// Nothing to do
}

void ParticleUpdatePass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	if (m_particleCount == 0)
		return;

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::computePassDebugColor, "Particles update pass");

	m_commandBuffer->bindPipeline(m_computePipeline.createConstNonOwnerResource());
	m_commandBuffer->bindDescriptorSet(m_descriptorSet.createConstNonOwnerResource(), 0, *m_computePipeline);

	constexpr VkExtent3D dispatchGroups = { 256, 1, 1 };
	const uint32_t groupSizeX = m_particleCount % dispatchGroups.width == 0 ? m_particleCount / dispatchGroups.width : (m_particleCount / dispatchGroups.width) + 1;
	constexpr uint32_t groupSizeY = 1;
	constexpr uint32_t groupSizeZ = 1;
	m_commandBuffer->dispatch(groupSizeX, groupSizeY, groupSizeZ);

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void ParticleUpdatePass::submit(const Wolf::SubmitContext& context)
{
	if (m_particleCount == 0)
		return;

	const std::vector<const Wolf::Semaphore*> waitSemaphores{  };
	const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };
	m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);

	if (m_computeShaderParser->compileIfFileHasBeenModified())
	{
		context.graphicAPIManager->waitIdle();
		createPipeline();
	}
}

void ParticleUpdatePass::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
	// Emitter info
	if (!m_emitterDrawInfoUpdates.empty())
	{
		for (const EmitterDrawInfoUpdate& emitterDrawInfoUpdate : m_emitterDrawInfoUpdates)
		{
			m_emitterDrawInfoBuffer->transferCPUMemoryWithStagingBuffer(&emitterDrawInfoUpdate.data, sizeof(EmitterDrawInfo), 0, emitterDrawInfoUpdate.emitterIdx * sizeof(EmitterDrawInfo));
		}

		m_emitterDrawInfoUpdates.clear();
	}

	if (globalTimer.getCurrentCachedMillisecondsDuration() > std::numeric_limits<uint32_t>::max())
		Wolf::Debug::sendCriticalError("Timer can't be stored in 32 bits");

	uint32_t globalTimerIn32bits = static_cast<uint32_t>(globalTimer.getCurrentCachedMillisecondsDuration());

	m_particleCount = 0;
	UniformBufferData uniformBufferData{};
	uniformBufferData.currentTimer = globalTimerIn32bits;

	for (uint32_t i = 0; i < m_emitters.size(); ++i)
	{
		const ParticleEmitter* emitter = m_emitters[i];
		uint32_t emitterMaxParticleCount = emitter->getMaxParticleCount();

		uniformBufferData.particleCountPerEmitter[i] = emitterMaxParticleCount;
		m_particleCount+= emitterMaxParticleCount;

		EmitterUpdateInfo& emitterInfo = uniformBufferData.emittersInfo[i];
		emitterInfo.direction = emitter->getDirection();
		emitterInfo.minSpeed = emitter->getMinSpeed() * (static_cast<float>(globalTimer.getElapsedTimeSinceLastUpdateInMs()) / 1000.0f);
		emitterInfo.maxSpeed = emitter->getMaxSpeed() * (static_cast<float>(globalTimer.getElapsedTimeSinceLastUpdateInMs()) / 1000.0f);
		emitterInfo.directionConeAngle = emitter->getDirectionConeAngle();

		emitterInfo.spawnPosition = emitter->getSpawnPosition();
		emitterInfo.spawnShape = emitter->getSpawnShape();
		emitterInfo.spawnShapeRadiusOrWidth = emitter->getSpawnShapeRadiusRadiusOrWidth();
		emitterInfo.spawnShapeHeight = emitter->getSpawnShapeHeight();
		emitterInfo.spawnBoxDepth = emitter->getSpawnBoxDepth();

		emitterInfo.minOrientationAngle = emitter->getOrientationMinAngle();
		emitterInfo.maxOrientationAngle = emitter->getOrientationMaxAngle();
		emitterInfo.minSizeMultiplier = emitter->getMinSizeMultiplier();
		emitterInfo.maxSizeMultiplier = emitter->getMaxSizeMultiplier();

		emitterInfo.minParticleLifetime = emitter->getMinLifetimeInMs();
		emitterInfo.maxParticleLifetime = emitter->getMaxLifetimeInMs();
		emitterInfo.emitterIdx = i;
		if (emitter->getNextSpawnTimerInMs() > std::numeric_limits<uint32_t>::max())
			Wolf::Debug::sendError("Next spawn timer can't be stored in 32 bits");
		emitterInfo.nextSpawnTimer = static_cast<uint32_t>(emitter->getNextSpawnTimerInMs());
		emitterInfo.nextParticleToSpawnIdx = emitter->getNextParticleToSpawnIdx();
		emitterInfo.nextParticleToSpawnCount = emitter->getNextParticleToSpawnCount();

		if (i == 1 && emitterInfo.nextParticleToSpawnCount == 0)
			emitterInfo.nextParticleToSpawnCount = 1;
	}

	m_uniformBuffer->transferCPUMemory(&uniformBufferData, sizeof(UniformBufferData));
}

void ParticleUpdatePass::registerEmitter(ParticleEmitter* emitter)
{
	m_emittersLock.lock();
	m_emitters.push_back(emitter);
	m_emittersLock.unlock();

	addEmitterInfoUpdate(emitter, static_cast<uint32_t>(m_emitters.size()) - 1u);
}

void ParticleUpdatePass::unregisterEmitter(ParticleEmitter* emitter)
{
	m_emittersLock.lock();
	std::erase(m_emitters, emitter);
	m_emittersLock.unlock();
}

void ParticleUpdatePass::updateEmitter(const ParticleEmitter* emitter)
{
	static constexpr uint32_t INVALID_IDX = MAX_EMITTER_COUNT + 1;
	uint32_t emitterIdx = INVALID_IDX;

	m_emittersLock.lock();
	for (uint32_t i = 0, end = static_cast<uint32_t>(m_emitters.size()); i < end; ++i)
	{
		if (m_emitters[i] == emitter)
		{
			emitterIdx = i;
			break;
		}
	}
	m_emittersLock.unlock();

	if (emitterIdx == INVALID_IDX)
	{
		Wolf::Debug::sendError("Emitter has not been registered");
		return;
	}

	addEmitterInfoUpdate(emitter, emitterIdx);
}

void ParticleUpdatePass::addEmitterInfoUpdate(const ParticleEmitter* emitter, uint32_t emitterIdx)
{
	EmitterDrawInfoUpdate emitterDrawInfoUpdate;

	emitterDrawInfoUpdate.data.materialIdx = emitter->getMaterialIdx();
	emitterDrawInfoUpdate.data.flipBookSizeX = emitter->getFlipBookSizeX();
	emitterDrawInfoUpdate.data.flipBookSizeY = emitter->getFlipBookSizeY();

	std::vector<float> opacity;
	emitter->computeOpacity(opacity, EmitterDrawInfo::OPACITY_VALUE_COUNT);
	memcpy(emitterDrawInfoUpdate.data.opacity, opacity.data(), sizeof(float) * EmitterDrawInfo::OPACITY_VALUE_COUNT);

	std::vector<float> size;
	emitter->computeSize(size, EmitterDrawInfo::SIZE_VALUE_COUNT);
	memcpy(emitterDrawInfoUpdate.data.size, size.data(), sizeof(float) * EmitterDrawInfo::SIZE_VALUE_COUNT);

	emitterDrawInfoUpdate.emitterIdx = emitterIdx;
	m_emitterDrawInfoUpdates.push_back(emitterDrawInfoUpdate);
}

void ParticleUpdatePass::createPipeline()
{
	Wolf::ShaderCreateInfo computeShaderInfo;
	computeShaderInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	m_computeShaderParser->readCompiledShader(computeShaderInfo.shaderCode);

	std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts = { m_descriptorSetLayout.createConstNonOwnerResource() };

	m_computePipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderInfo, descriptorSetLayouts));
}

void ParticleUpdatePass::createNoiseBuffer()
{
	static std::default_random_engine generator;
	static std::uniform_real_distribution distrib(0.0f, 1.0f);

	float randomData[NOISE_POINT_COUNT];
	for (float& random : randomData)
	{
		random = distrib(generator);
	}

	m_noiseBuffer.reset(Wolf::Buffer::createBuffer(NOISE_POINT_COUNT * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	m_noiseBuffer->transferCPUMemoryWithStagingBuffer(randomData, NOISE_POINT_COUNT * sizeof(float));
}
