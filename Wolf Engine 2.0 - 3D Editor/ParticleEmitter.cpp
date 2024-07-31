#include "ParticleEmitter.h"

#include "EditorParamsHelper.h"
#include "Entity.h"
#include "Particle.h"
#include "ParticleUpdatePass.h"

ParticleEmitter::ParticleEmitter(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback) :
	m_particleUpdatePass(renderingPipeline->getParticleUpdatePass()), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
	m_maxParticleCount = 1;
	m_delayBetweenTwoParticles = 1.0f;
	m_direction = glm::vec3(0.0f, 1.0f, 0.0f);
}

ParticleEmitter::~ParticleEmitter()
{
	m_particleUpdatePass->unregisterEmitter(this);
}

void ParticleEmitter::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_editorParams);

	m_particleUpdatePass->registerEmitter(this);
}

void ParticleEmitter::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void ParticleEmitter::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void ParticleEmitter::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
	// Set next idx from previous spawn
	m_nextParticleToSpawnIdx = (m_nextParticleToSpawnIdx + m_nextParticleToSpawnCount) % m_maxParticleCount;

	uint64_t msToWaitBetweenTwoParticles = static_cast<uint64_t>(static_cast<float>(m_delayBetweenTwoParticles) * 1000.0f);

	uint64_t msElapsedSinceLastSpawn = globalTimer.getCurrentCachedMillisecondsDuration() - m_lastSpawnMsTimer;
	m_nextParticleToSpawnCount = msToWaitBetweenTwoParticles == 0 ? m_maxParticleCount : static_cast<uint32_t>(msElapsedSinceLastSpawn / msToWaitBetweenTwoParticles);
	if (m_nextParticleToSpawnCount > 0)
	{
		m_nextSpawnMsTimer = m_lastSpawnMsTimer + msToWaitBetweenTwoParticles;
		m_lastSpawnMsTimer += msToWaitBetweenTwoParticles * m_nextParticleToSpawnCount;
	}

	if (!m_particleNotificationRegistered && m_particleEntity)
	{
		if (const Wolf::ResourceNonOwner<Particle> particleComponent = (*m_particleEntity)->getComponent<Particle>())
		{
			particleComponent->subscribe(this, [this]() { onParticleDataChanged(); });
		}
	}
}

void ParticleEmitter::updateNormalizedDirection()
{
	if (static_cast<glm::vec3>(m_direction) != glm::vec3(0.0f))
	{
		m_normalizedDirection = glm::normalize(static_cast<glm::vec3>(m_direction));
	}
}

void ParticleEmitter::onParticleOpacityChanged()
{
	m_particleUpdatePass->updateEmitter(this);
}

void ParticleEmitter::onParticleEntityChanged()
{
	m_particleEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_particleEntityParam)));
}

void ParticleEmitter::onParticleDataChanged()
{
	const Wolf::ResourceNonOwner<Particle> particleComponent = (*m_particleEntity)->getComponent<Particle>();
	m_materialIdx = particleComponent->getMaterialIdx();

	m_particleUpdatePass->updateEmitter(this);
}
