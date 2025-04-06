#include "ParticleEmitter.h"

#include "EditorParamsHelper.h"
#include "Entity.h"
#include "ParticleUpdatePass.h"

ParticleEmitter::ParticleEmitter(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback, const std::function<void(ComponentInterface*)>& requestReloadCallback) :
	m_particleUpdatePass(renderingPipeline->getParticleUpdatePass()), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
	m_requestReloadCallback = requestReloadCallback;

	m_particleUpdatePass->registerEmitter(this);

	m_maxParticleCount = 1;
	m_delayBetweenTwoParticles = 1.0f;
	m_directionConeDirection = glm::vec3(0.0f, 1.0f, 0.0f);
	m_particleMinSizeMultiplier = 1.0f;
	m_particleMaxSizeMultiplier = 1.0f;
	m_isEmitting = true;
	m_particleColor = glm::vec3(1.0f);
}

ParticleEmitter::~ParticleEmitter()
{
	m_particleUpdatePass->unregisterEmitter(this);
}

void ParticleEmitter::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_allEditorParams);
}

void ParticleEmitter::activateParams()
{
	std::string unused;
	forAllVisibleParams([](EditorParamInterface* e, std::string&) { e->activate(); }, unused);
}

void ParticleEmitter::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	forAllVisibleParams([tabCount](EditorParamInterface* e, std::string& inOutJSON) mutable  { e->addToJSON(inOutJSON, tabCount, false); }, outJSON);
}

void ParticleEmitter::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	if (m_needCheckForNewLinkedEntities)
	{
		onParticleEntityChanged();
		m_needCheckForNewLinkedEntities = false;
	}

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

	if (!m_isEmitting)
	{
		m_nextParticleToSpawnCount = 0;
	}

	if (!m_particleNotificationRegistered && m_particleComponent)
	{
		m_particleComponent->subscribe(this, [this](Flags) { onParticleDataChanged(); });
		m_particleNotificationRegistered = true;

		onParticleDataChanged();
	}
}

void ParticleEmitter::setSpawnPosition(const glm::vec3& position)
{
	switch (static_cast<uint32_t>(m_spawnShape))
	{
	case SPAWN_CYLINDER_SHAPE:
		m_spawnCylinderCenterPosition = position;
		break;
	case SPAWN_BOX_SHAPE:
		m_spawnBoxCenterPosition = position;
		break;
	default:
		Wolf::Debug::sendError("Undefined spawn shape");
	}
}

void ParticleEmitter::setDirection(const glm::vec3& direction)
{
	switch (static_cast<uint32_t>(m_directionShape))
	{
	case DIRECTION_CONE_SHAPE:
	{
		m_directionConeDirection = direction;
		break;
	}
	default:
		Wolf::Debug::sendError("Undefined direction shape");
		break;
	}
}

glm::vec3 ParticleEmitter::getSpawnPosition() const
{
	switch (static_cast<uint32_t>(m_spawnShape))
	{
		case SPAWN_CYLINDER_SHAPE:
			return m_spawnCylinderCenterPosition;
		case SPAWN_BOX_SHAPE:
			return m_spawnBoxCenterPosition;
		default:
			Wolf::Debug::sendError("Undefined spawn shape");
	}

	return glm::vec3(0.0f);
}

uint32_t ParticleEmitter::getSpawnShape() const
{
	return m_spawnShape;
}

float ParticleEmitter::getSpawnShapeRadiusRadiusOrWidth() const
{
	switch (static_cast<uint32_t>(m_spawnShape))
	{
	case SPAWN_CYLINDER_SHAPE:
		return m_spawnCylinderRadius;
	case SPAWN_BOX_SHAPE:
		return m_spawnBoxWidth;
	default:
		Wolf::Debug::sendError("Undefined spawn shape");
	}

	return 0.0f;
}

float ParticleEmitter::getSpawnShapeHeight() const
{
	switch (static_cast<uint32_t>(m_spawnShape))
	{
	case SPAWN_CYLINDER_SHAPE:
		return m_spawnCylinderHeight;
	case SPAWN_BOX_SHAPE:
		return m_spawnBoxHeight;
	default:
		Wolf::Debug::sendError("Undefined spawn shape");
	}

	return 0.0f;
}

float ParticleEmitter::getSpawnBoxDepth() const
{
	return m_spawnBoxDepth;
}

void ParticleEmitter::onEntityRegistered()
{
	m_needCheckForNewLinkedEntities = true;
}

void ParticleEmitter::forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString)
{
	for (EditorParamInterface* editorParam : m_alwaysVisibleEditorParams)
	{
		callback(editorParam, inOutString);
	}

	// Shape specific params
	switch (static_cast<uint32_t>(m_spawnShape))
	{
		case SPAWN_CYLINDER_SHAPE:
		{
			for (EditorParamInterface* editorParam : m_spawnShapeCylinderParams)
			{
				callback(editorParam, inOutString);
			}
			break;
		}
		case SPAWN_BOX_SHAPE:
		{
			for (EditorParamInterface* editorParam : m_spawnShapeBoxParams)
			{
				callback(editorParam, inOutString);
			}
			break;
		}
		default:
			Wolf::Debug::sendError("Undefined spawn shape");
			break;
	}

	switch (static_cast<uint32_t>(m_directionShape))
	{
		case DIRECTION_CONE_SHAPE:
		{
			for (EditorParamInterface* editorParam : m_directionConeParams)
			{
				callback(editorParam, inOutString);
			}
			break;
		}
		default:
			Wolf::Debug::sendError("Undefined direction shape");
			break;
	}
}

void ParticleEmitter::onUsedTileCountInFlipBookChanged()
{
	m_firstFlipBookRandomRange = std::min(static_cast<uint32_t>(m_firstFlipBookRandomRange), m_flipBookSizeX * m_flipBookSizeY - m_usedTileCountInFlipBook);
	m_firstFlipBookRandomRange.setMax(m_flipBookSizeX * m_flipBookSizeY - m_usedTileCountInFlipBook);

	m_particleUpdatePass->updateEmitter(this);

	m_requestReloadCallback(this);
}

void ParticleEmitter::updateNormalizedDirection()
{
	if (static_cast<glm::vec3>(m_directionConeDirection) != glm::vec3(0.0f))
	{
		m_normalizedDirection = glm::normalize(static_cast<glm::vec3>(m_directionConeDirection));
	}
}

void ParticleEmitter::onParticleSizeChanged()
{
	m_particleUpdatePass->updateEmitter(this);
}

void ParticleEmitter::onParticleOpacityChanged()
{
	m_particleUpdatePass->updateEmitter(this);
}

void ParticleEmitter::onParticleEntityChanged()
{
	if (!m_entity)
		return;

	if (!static_cast<std::string>(m_particleEntityParam).empty())
	{
		if (Wolf::NullableResourceNonOwner<Particle> particleComponent = m_getEntityFromLoadingPathCallback(m_particleEntityParam)->getComponent<Particle>())
		{
			m_particleComponent = particleComponent;
			//m_usedTileCountInFlipBook = 0; // will be set to max in 'onParticleDataChanged'
			m_particleNotificationRegistered = false;
		}
	}
	else
	{
		m_particleComponent = Wolf::NullableResourceNonOwner<Particle>();
		m_firstFlipBookIdx.setMax(1);
	}
}

void ParticleEmitter::onParticleDataChanged()
{
	m_materialIdx = m_particleComponent->getMaterialIdx();
	m_flipBookSizeX = m_particleComponent->getFlipBookSizeX();
	m_flipBookSizeY = m_particleComponent->getFlipBookSizeY();
	m_firstFlipBookIdx.setMax(m_flipBookSizeX * m_flipBookSizeY);
	m_firstFlipBookRandomRange.setMax(m_flipBookSizeX * m_flipBookSizeY);
	m_usedTileCountInFlipBook.setMax(m_flipBookSizeX * m_flipBookSizeY);
	if (m_usedTileCountInFlipBook == 0)
	{
		m_usedTileCountInFlipBook = m_flipBookSizeX * m_flipBookSizeY;
	}

	m_particleUpdatePass->updateEmitter(this);

	m_requestReloadCallback(this);
}
