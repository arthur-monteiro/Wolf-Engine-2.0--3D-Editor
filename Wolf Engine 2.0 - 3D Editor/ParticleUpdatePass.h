#pragma once

#include <glm/glm.hpp>

#include "CommandRecordBase.h"
#include "DescriptorSetLayoutGenerator.h"
#include "ParticleEmitter.h"
#include "ResourceUniqueOwner.h"
#include "ShaderParser.h"

class ParticleUpdatePass : public Wolf::CommandRecordBase
{
public:
	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer);

	uint32_t getParticleCount() const { return m_particleCount; }
	const Wolf::Buffer& getParticleBuffer() const { return *m_particlesBuffer; }
	const Wolf::Buffer& getEmittersBuffer() const { return *m_emitterDrawInfoBuffer; }

	void registerEmitter(ParticleEmitter* emitter);
	void unregisterEmitter(ParticleEmitter* emitter);

	void updateEmitter(const ParticleEmitter* emitter);

private:
	void createPipeline();
	void createNoiseBuffer();

	void addEmitterInfoUpdate(const ParticleEmitter* emitter, uint32_t emitterIdx);

	Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_computeShaderParser;
	Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_computePipeline;

	// Particles buffer
	static constexpr uint32_t MAX_TOTAL_PARTICLES = 262'144;
	struct ParticleInfoGPU
	{
		glm::vec3 position;
		uint32_t createdTimer;

		uint32_t emitterIdx;
		float age;
		float orientationAngle;
		float sizeMultiplier;
	};
	static_assert(sizeof(ParticleInfoGPU) == 32);
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_particlesBuffer;

	// Emitter draw info buffer
	static constexpr uint32_t MAX_EMITTER_COUNT = 16;
	struct EmitterDrawInfo
	{
		uint32_t materialIdx;

		static constexpr uint32_t OPACITY_VALUE_COUNT = 32;
		float opacity[OPACITY_VALUE_COUNT];

		static constexpr uint32_t SIZE_VALUE_COUNT = 32;
		float size[OPACITY_VALUE_COUNT];

		uint32_t flipBookSizeX;
		uint32_t flipBookSizeY;
	};
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_emitterDrawInfoBuffer;

	struct EmitterDrawInfoUpdate
	{
		EmitterDrawInfo data;
		uint32_t emitterIdx;
	};
	std::vector<EmitterDrawInfoUpdate> m_emitterDrawInfoUpdates;

	// Update
	struct EmitterUpdateInfo
	{
		glm::vec3 direction;
		uint32_t nextParticleToSpawnIdx;

		glm::vec3 spawnPosition;
		uint32_t nextParticleToSpawnCount;

		uint32_t spawnShape;
		float spawnShapeRadiusOrWidth;
		float spawnShapeHeight;
		float spawnBoxDepth;

		uint32_t minParticleLifetime;
		uint32_t emitterIdx;
		uint32_t nextSpawnTimer;
		float directionConeAngle;

		float minSpeed;
		float maxSpeed;
		uint32_t maxParticleLifetime;
		float minOrientationAngle;

		float maxOrientationAngle;
		float minSizeMultiplier;
		float maxSizeMultiplier;
		float padding;
	};

	struct UniformBufferData
	{
		uint32_t currentTimer;
		glm::vec3 padding;

		uint32_t particleCountPerEmitter[MAX_EMITTER_COUNT];
		static_assert((sizeof(uint32_t) * MAX_EMITTER_COUNT) % sizeof(glm::vec4) == 0);

		EmitterUpdateInfo emittersInfo[MAX_EMITTER_COUNT];
		static_assert(sizeof(EmitterUpdateInfo) % sizeof(glm::vec4) == 0);
	};
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_uniformBuffer;

	// Noise
	static constexpr uint32_t NOISE_POINT_COUNT = 1024;
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_noiseBuffer;

	uint32_t m_particleCount = 0;

	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

	std::vector<ParticleEmitter*> m_emitters;
	std::mutex m_emittersLock;
};

