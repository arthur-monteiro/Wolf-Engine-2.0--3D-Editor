#pragma once

#include <glm/glm.hpp>

#include <CommandRecordBase.h>
#include <DescriptorSetLayoutGenerator.h>
#include <ResourceUniqueOwner.h>
#include <ShaderParser.h>

#include "ParticleEmitter.h"
#include "ShadowMaskPassCascadedShadowMapping.h"

class ParticleUpdatePass : public Wolf::CommandRecordBase
{
public:
	static constexpr uint32_t NOISE_POINT_COUNT = 262144;

	ParticleUpdatePass(const Wolf::ResourceNonOwner<ShadowMaskPassCascadedShadowMapping>& shadowMaskPassCascadedShadowMapping);

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<UpdateGPUBuffersPass>& updateGPUBuffersPass);

	uint32_t getParticleCount() const { return m_particleCount; }
	const Wolf::Buffer& getParticleBuffer() const { return *m_particlesBuffer; }
	const Wolf::Buffer& getEmittersBuffer() const { return *m_emitterDrawInfoBuffer; }
	const Wolf::Buffer& getNoiseBuffer() const { return *m_noiseBuffer; }

	void registerEmitter(ParticleEmitter* emitter);
	void unregisterEmitter(ParticleEmitter* emitter);

	void updateEmitter(const ParticleEmitter* emitter);

	uint32_t registerDepthTexture(const Wolf::ResourceNonOwner<Wolf::Image>& depthImage);

private:
	Wolf::ResourceNonOwner<ShadowMaskPassCascadedShadowMapping> m_shadowMaskPassCascadedShadowMapping;

	void createPipeline();
	void createNoiseBuffer();

	void addEmitterInfoUpdate(const ParticleEmitter* emitter, uint32_t emitterIdx);
	uint32_t addDepthTexturesToBindless(const std::vector<Wolf::DescriptorSetGenerator::ImageDescription>& images);

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

		glm::vec3 direction;
		glm::vec3 color;
	};
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_particlesBuffer;

	// Emitter draw info buffer
	static constexpr uint32_t MAX_EMITTER_COUNT = 16;
	struct EmitterDrawInfo
	{
		uint32_t materialIdx;

		static constexpr uint32_t OPACITY_VALUE_COUNT = 32;
		float opacity[OPACITY_VALUE_COUNT];

		static constexpr uint32_t SIZE_VALUE_COUNT = 32;
		float size[SIZE_VALUE_COUNT];

		uint32_t flipBookSizeX;
		uint32_t flipBookSizeY;
		uint32_t firstFlipBookIdx;
		uint32_t firstFlipBookRandomRange;
		uint32_t usedTileCountInFlipBook;
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
		float delayBetweenTwoParticlesInMs;

		glm::vec3 color;
		uint32_t collisionType;

		glm::mat4 collisionViewProjMatrix;

		uint32_t collisionDepthTextureIdx;
		float collisionDepthScale;
		float collisionDepthOffset;
		uint32_t collisionBehaviour;
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
	Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;

	// Noise
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_noiseBuffer;

	uint32_t m_particleCount = 0;

	static constexpr uint32_t MAX_DEPTH_IMAGES = 8;
	uint32_t m_currentDepthTextureBindlessCount = 0;
	Wolf::ResourceUniqueOwner<Wolf::Image> m_defaultDepthImage;
	Wolf::ResourceUniqueOwner<Wolf::Sampler> m_depthSampler;
	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

	std::vector<ParticleEmitter*> m_emitters;
	std::mutex m_emittersLock;
};

