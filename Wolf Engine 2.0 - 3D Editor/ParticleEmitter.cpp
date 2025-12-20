#include "ParticleEmitter.h"

#include "CustomDepthPass.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "ParticleUpdatePass.h"

ParticleEmitter::ParticleEmitter(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback, const std::function<void(ComponentInterface*)>& requestReloadCallback) :
	m_particleUpdatePass(renderingPipeline->getParticleUpdatePass()), m_customDepthPass(renderingPipeline->getCustomDepthPass()), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
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

	m_collisionDepthTextureSize = 1024;
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

	float msToWaitBetweenTwoParticles = m_delayBetweenTwoParticles * 1000.0f;

	uint64_t msElapsedSinceLastSpawn = globalTimer.getCurrentCachedMillisecondsDuration() - m_lastSpawnMsTimer;
	m_nextParticleToSpawnCount = static_cast<uint32_t>(static_cast<float>(msElapsedSinceLastSpawn) / msToWaitBetweenTwoParticles);
	if (m_nextParticleToSpawnCount > 0)
	{
		m_nextSpawnMsTimer = m_lastSpawnMsTimer + static_cast<uint64_t>(msToWaitBetweenTwoParticles);
		m_lastSpawnMsTimer += static_cast<uint64_t>(msToWaitBetweenTwoParticles * static_cast<float>(m_nextParticleToSpawnCount));
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

	if (m_needToRegisterDepthTexture)
	{
		m_collisionDepthTextureIdx = m_particleUpdatePass->registerDepthTexture(m_depthCollisionImage.createNonOwnerResource());
		m_particleUpdatePass->updateEmitter(this);

		m_needToRegisterDepthTexture = false;
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

	switch (static_cast<uint32_t>(m_collisionType))
	{
		case COLLISION_TYPE_NONE:
			break;
		case COLLISION_TYPE_DEPTH:
		{
			for (EditorParamInterface* editorParam : m_collisionDepthParams)
			{
				callback(editorParam, inOutString);
			}
			break;
		}
		default:
			Wolf::Debug::sendError("Undefined collision type");
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

void ParticleEmitter::onParticleCountChanged()
{
	if (m_maxParticleCount > ParticleUpdatePass::NOISE_POINT_COUNT)
	{
		Wolf::Debug::sendWarning("Particle count should not exceed ParticleUpdatePass::NOISE_POINT_COUNT(" + std::to_string(ParticleUpdatePass::NOISE_POINT_COUNT) + ")");
	}
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

void ParticleEmitter::onCollisionTypeChanged()
{
	if (m_collisionType == COLLISION_TYPE_DEPTH)
	{
		if (!m_depthCollisionImage || m_depthCollisionImage->getExtent().width != m_collisionDepthTextureSize)
		{
			createDepthCollisionImage();
			createDepthCollisionCamera();

			CustomDepthPass::Request request(m_depthCollisionImage.createNonOwnerResource(), m_depthCollisionCamera.createNonOwnerResource<Wolf::CameraInterface>());
			m_customDepthPass->addRequestBeforeFrame(request);

			m_needToRegisterDepthTexture = true;
		}
	}

	m_requestReloadCallback(this);
}

void ParticleEmitter::createDepthCollisionImage()
{
	Wolf::CreateImageInfo createImageInfo{};
	createImageInfo.extent = { m_collisionDepthTextureSize, m_collisionDepthTextureSize, 1 };
	createImageInfo.format = Wolf::Format::D32_SFLOAT;
	createImageInfo.usage = Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | Wolf::ImageUsageFlagBits::SAMPLED;
	createImageInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	createImageInfo.mipLevelCount = 1;
	m_depthCollisionImage.reset(Wolf::Image::createImage(createImageInfo));
}

void ParticleEmitter::createDepthCollisionCamera()
{
	m_collisionDepthScale = 50.0f;
	m_collisionDepthOffset = static_cast<glm::vec3>(m_spawnBoxCenterPosition).y - 50.0f;
	m_depthCollisionCamera.reset(new Wolf::OrthographicCamera(static_cast<glm::vec3>(m_spawnBoxCenterPosition) - glm::vec3(0.0f, 25.0f, 0.0f), m_spawnBoxWidth * 0.5, 25.0f, glm::vec3(0.0f, -1.0f, 0.0f), 0.0f, 50.0f));
}
