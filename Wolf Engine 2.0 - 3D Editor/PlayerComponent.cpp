#include "PlayerComponent.h"

#include <glm/gtc/constants.hpp>

#include "AnimatedModel.h"
#include "ContaminationEmitter.h"
#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "UpdateGPUBuffersPass.h"

PlayerComponent::PlayerComponent(std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback, const Wolf::ResourceNonOwner<EntityContainer>& entityContainer, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
	: m_getEntityFromLoadingPathCallback(std::move(getEntityFromLoadingPathCallback)), m_entityContainer(entityContainer), m_updateGPUBuffersPass(renderingPipeline->getUpdateGPUBuffersPass())
{
}

void PlayerComponent::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_editorParams);
}

void PlayerComponent::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void PlayerComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void PlayerComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	if (!m_entity)
		Wolf::Debug::sendCriticalError("Entity not set");

	// Move player
	float moveX, moveY;
	inputHandler->getJoystickSpeedForGamepad(static_cast<uint8_t>(m_gamepadIdx), 0, moveX, moveY);

	const auto now = std::chrono::high_resolution_clock::now();
	const long long millisecondsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_lastUpdateTimePoint).count();
	const float secondDuration = static_cast<float>(millisecondsDuration) / 1000.0f;
	m_lastUpdateTimePoint = now;

	glm::vec3 previousPosition = m_entity->getPosition();

	float speed = m_speed;
	bool isRunning = false;
	if (inputHandler->getTriggerValueForGamepad(static_cast<uint8_t>(m_gamepadIdx), 1) > 0.0f)
	{
		speed *= 2.0f;
		isRunning = true;
	}

	glm::vec3 position = m_entity->getPosition();
	position.x += moveX * secondDuration * speed;
	position.z += moveY * secondDuration * speed;
	m_entity->setPosition(position);

	if (moveX != 0.0f || moveY != 0.0f)
	{
		if (m_animatedModel)
		{
			if (isRunning)
			{
				(*m_animatedModel)->setAnimation(m_animationRun);
			}
			else
			{
				(*m_animatedModel)->setAnimation(m_animationWalk);
			}
		}

		glm::vec3 direction = glm::normalize(position - previousPosition);
		float phi = glm::asin(direction.y);
		float theta = glm::asin(direction.x / glm::cos(phi));

		if (direction.z < 0.0f)
		{
			theta = glm::pi<float>() - theta;
		}

		m_entity->setRotation(glm::vec3(phi, theta, 0.0f));
	}
	else
	{
		if (m_animatedModel)
		{
			(*m_animatedModel)->setAnimation(m_animationIdle);
		}
	}

	// Shoot
	inputHandler->getJoystickSpeedForGamepad(static_cast<uint8_t>(m_gamepadIdx), 1, m_currentShootX, m_currentShootY);

	if (isShooting())
	{
		m_shootDebugMeshRebuildRequested = true;

		if (m_smokeEmitterComponent)
		{
			m_smokeEmitterComponent->requestEmission();
			m_smokeEmitterComponent->setSpawnPosition(getGunPosition());
			m_smokeEmitterComponent->setColor(m_gasCylinderComponent->getColor());

			const glm::vec3 shootDirection = glm::normalize(glm::vec3(m_currentShootX, 0, m_currentShootY));
			m_smokeEmitterComponent->setDirection(shootDirection);
		}
	}
	else
	{
		if (m_smokeEmitterComponent)
		{
			m_smokeEmitterComponent->stopEmission();
		}
	}

	// Take or drop gas cylinder
	if (m_isWaitingForGasCylinderButtonRelease)
	{
		if (!inputHandler->isGamepadButtonPressed(static_cast<uint8_t>(m_gamepadIdx), 2))
		{
			m_isWaitingForGasCylinderButtonRelease = false;
		}
	}
	else if (inputHandler->isGamepadButtonPressed(static_cast<uint8_t>(m_gamepadIdx), 2))
	{
		m_isWaitingForGasCylinderButtonRelease = true;

		if (!static_cast<std::string>(m_gasCylinderParam).empty())
		{
			m_gasCylinderParam = "";
		}
		else
		{
			std::vector<Wolf::ResourceNonOwner<Entity>> entities;
			m_entityContainer->findEntitiesWithCenterInSphere({ m_entity->getBoundingSphere().getCenter(), 1.0f }, entities);

			float minDistance = 1.0f;
			std::unique_ptr<Wolf::ResourceNonOwner<Entity>> nearestGasCylinderEntity;
			for (const Wolf::ResourceNonOwner<Entity>& entity : entities)
			{
				float distance = glm::distance(entity->getBoundingSphere().getCenter(), m_entity->getBoundingSphere().getCenter());
				if (distance < minDistance && entity->getComponent<GasCylinderComponent>())
				{
					minDistance = distance;
					nearestGasCylinderEntity.reset(new Wolf::ResourceNonOwner<Entity>(entity));
				}
			}

			if (nearestGasCylinderEntity)
			{
				m_gasCylinderParam = (*nearestGasCylinderEntity)->getLoadingPath();
			}
		}
	}

	if (m_needCheckForNewLinkedEntities)
	{
		onGasCylinderChanged();
		onSmokeEmitterChanged();
		m_needCheckForNewLinkedEntities = false;
	}

	if (isShooting())
	{
		// Send info to contamination emitter to update the 3D texture
		uint64_t elapsedTimeSinceLastRequest = globalTimer.getCurrentCachedMillisecondsDuration() - m_lastShootRequestSentToContaminationEmitterTimer;
		static constexpr uint64_t MIN_DELAY_MS = 250;
		if (elapsedTimeSinceLastRequest > MIN_DELAY_MS)
		{
			if (m_contaminationEmitterEntity)
			{
				if (const Wolf::ResourceNonOwner<ContaminationEmitter> contaminationEmitterComponent = (*m_contaminationEmitterEntity)->getComponent<ContaminationEmitter>())
				{
					contaminationEmitterComponent->addShootRequest(ShootRequest{ getGunPosition(), glm::normalize(glm::vec3(m_currentShootX, 0, m_currentShootY)), m_shootLength, 
						glm::radians(static_cast<float>(m_shootAngle)), getMaxParticleSize(), getGasCylinderCleanFlags(), static_cast<uint32_t>(globalTimer.getCurrentCachedMillisecondsDuration())});
				}
			}
			m_lastShootRequestSentToContaminationEmitterTimer = globalTimer.getCurrentCachedMillisecondsDuration();
		}

		// Send info to gas cylinder to update current storage
		if (m_gasCylinderComponent)
		{
			m_gasCylinderComponent->addShootRequest(globalTimer);
		}
	}

	if (m_animatedModel && m_gasCylinderComponent && !(*m_animatedModel)->getBoneNamesAndIndices().empty())
	{
		const std::vector<std::pair<std::string, uint32_t>>& boneNamesAndIndices = (*m_animatedModel)->getBoneNamesAndIndices();

		glm::vec3 topBonePosition = (*m_animatedModel)->getBonePosition(boneNamesAndIndices[m_gasCylinderTopBone].second);
		glm::vec3 botBonePosition = (*m_animatedModel)->getBonePosition(boneNamesAndIndices[m_gasCylinderBottomBone].second);

		glm::mat3 modelTransform = (*m_animatedModel)->computeRotationMatrix();
		glm::vec3 topBoneOffset = modelTransform * m_gasCylinderTopBoneOffset;
		glm::vec3 botBoneOffset = modelTransform * m_gasCylinderBottomBoneOffset;

		m_gasCylinderComponent->setLinkPositions(topBonePosition + topBoneOffset, botBonePosition + botBoneOffset);
	}
}

void PlayerComponent::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	if (isShooting())
	{
		if (m_shootDebugMeshRebuildRequested)
			buildShootDebugMesh();

		DebugRenderingManager::LinesUBData uniformData{};
		uniformData.transform = glm::mat4(1.0f);
		debugRenderingManager.addCustomGroupOfLines(m_shootDebugMesh.createNonOwnerResource(), uniformData);
	}
}

void PlayerComponent::onEntityRegistered()
{
	updateAnimatedModel();
	m_entity->subscribe(this, [this](Flags) { updateAnimatedModel(); });

	m_needCheckForNewLinkedEntities = true;

	buildShootDebugMesh();
}

void PlayerComponent::updateAnimatedModel()
{
	if (Wolf::NullableResourceNonOwner<AnimatedModel> animatedModel = m_entity->getComponent<AnimatedModel>())
	{
		m_animatedModel.reset(new Wolf::ResourceNonOwner<AnimatedModel>(animatedModel));

		animatedModel->unsubscribe(this);
		animatedModel->subscribe(this, [this](Flags) { updateAnimatedModel(); });

		std::vector<std::string> options;
		animatedModel->getAnimationOptions(options);

		m_animationIdle.setOptions(options);
		m_animationWalk.setOptions(options);
		m_animationRun.setOptions(options);

		const std::vector<std::pair<std::string, uint32_t>>& boneNamesAndIndices = animatedModel->getBoneNamesAndIndices();
		std::vector<std::string> boneNames(boneNamesAndIndices.size());
		for (uint32_t i = 0; i < boneNames.size(); ++i)
		{
			boneNames[i] = boneNamesAndIndices[i].first;
		}
		m_gasCylinderTopBone.setOptions(boneNames);
		m_gasCylinderBottomBone.setOptions(boneNames);
	}
}

void PlayerComponent::onContaminationEmitterChanged()
{
	m_contaminationEmitterEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_contaminationEmitterParam)));
}

void PlayerComponent::onSmokeEmitterChanged()
{
	if (!m_entity)
		return;

	if (const std::string& smokeEmitterStr = m_smokeEmitterParam; !smokeEmitterStr.empty())
	{
		Wolf::ResourceNonOwner<Entity> entity = m_getEntityFromLoadingPathCallback(smokeEmitterStr);
		if (Wolf::NullableResourceNonOwner<ParticleEmitter> particleEmitter = entity->getComponent<ParticleEmitter>())
		{
			m_smokeEmitterComponent = particleEmitter;
		}
	}
	else
	{
		m_smokeEmitterComponent = Wolf::NullableResourceNonOwner<ParticleEmitter>();
	}
}

void PlayerComponent::onGasCylinderChanged()
{
	if (!m_entity)
		return;

	if (static_cast<std::string>(m_gasCylinderParam).empty())
	{
		if (m_gasCylinderComponent)
		{
			m_gasCylinderComponent->onPlayerRelease();
		}
		m_gasCylinderComponent = Wolf::NullableResourceNonOwner<GasCylinderComponent>();
	}
	else if (Wolf::NullableResourceNonOwner<GasCylinderComponent> gasCylinder = m_getEntityFromLoadingPathCallback(m_gasCylinderParam)->getComponent<GasCylinderComponent>())
	{
		m_gasCylinderComponent = gasCylinder;
	}
	else
	{
		Wolf::Debug::sendError("Entity linked to player component doesn't have a gas cylinder component");
		m_gasCylinderComponent = Wolf::NullableResourceNonOwner<GasCylinderComponent>();
	}
}

glm::vec3 PlayerComponent::getGunPosition() const
{
	return m_entity->getPosition() + static_cast<glm::vec3>(m_gunPositionOffset);
}

bool PlayerComponent::canShoot() const
{
	return m_gasCylinderComponent && !m_gasCylinderComponent->isEmpty();
}

bool PlayerComponent::isShooting() const
{
	return canShoot() && (m_currentShootX != 0.0f || m_currentShootY != 0.0f);
}

uint32_t PlayerComponent::getGasCylinderCleanFlags() const
{
	return m_gasCylinderComponent->getCleanFlags();
}

float PlayerComponent::getMaxParticleSize() const
{
	return m_smokeEmitterComponent->getMaxSizeMultiplier();
}

void PlayerComponent::buildShootDebugMesh()
{
	const glm::vec3 gunPosition = getGunPosition();
	const glm::vec3 shootDirection = glm::normalize(glm::vec3(m_currentShootX, 0, m_currentShootY));

	std::vector<DebugRenderingManager::DebugVertex> vertices;
	std::vector<uint32_t> indices;

	// Shoot direction
	vertices.emplace_back(gunPosition);
	vertices.emplace_back(gunPosition + glm::vec3(shootDirection) * static_cast<float>(m_shootLength));

	// Cone neg Y
	vertices.emplace_back(gunPosition);
	vertices.emplace_back(gunPosition + glm::vec3(shootDirection) * static_cast<float>(m_shootLength) 
		+ glm::vec3(0.0f, -1.0f, 0.0f) * static_cast<float>(m_shootLength) * glm::tan(glm::radians(static_cast<float>(m_shootAngle))));

	// Cone pos Y
	vertices.emplace_back(gunPosition);
	vertices.emplace_back(gunPosition + glm::vec3(shootDirection) * static_cast<float>(m_shootLength)
		+ glm::vec3(0.0f, 1.0f, 0.0f) * static_cast<float>(m_shootLength) * glm::tan(glm::radians(static_cast<float>(m_shootAngle))));

	// Cone neg tangent
	vertices.emplace_back(gunPosition);
	vertices.emplace_back(gunPosition + glm::vec3(shootDirection) * static_cast<float>(m_shootLength)
		+ glm::vec3(m_currentShootY, 0.0f, -m_currentShootX) * static_cast<float>(m_shootLength) * glm::tan(glm::radians(static_cast<float>(m_shootAngle))));

	// Cone pos tangent
	vertices.emplace_back(gunPosition);
	vertices.emplace_back(gunPosition + glm::vec3(shootDirection) * static_cast<float>(m_shootLength)
		+ glm::vec3(-m_currentShootY, 0.0f, m_currentShootX) * static_cast<float>(m_shootLength) * glm::tan(glm::radians(static_cast<float>(m_shootAngle))));

	// Cone end circle
	const glm::vec3 circleOrigin = glm::vec3(gunPosition + glm::vec3(shootDirection) * static_cast<float>(m_shootLength));
	const float circleRadius = static_cast<float>(m_shootLength) * glm::tan(glm::radians(static_cast<float>(m_shootAngle)));
	constexpr uint32_t CIRCLE_VERTEX_COUNT = 32;

	for (uint32_t i = 0; i <= CIRCLE_VERTEX_COUNT; ++i)
	{
		const float circleRad = (static_cast<float>(i) / static_cast<float>(CIRCLE_VERTEX_COUNT)) * glm::two_pi<float>();

		const float circleX = glm::cos(circleRad) * circleRadius;
		const float circleY = glm::sin(circleRad) * circleRadius;

		vertices.emplace_back(circleOrigin + circleX * glm::vec3(m_currentShootY, 0.0f, -m_currentShootX) + circleY * glm::vec3(0.0f, 1.0f, 0.0f));
		if (i != 0)
			vertices.push_back(vertices.back());
	}

	for (uint32_t i = 0; i < vertices.size(); ++i)
	{
		indices.push_back(i);
	}

	if (!m_shootDebugMesh)
	{
		m_shootDebugMesh.reset(new Wolf::Mesh(vertices, indices));
	}
	else
	{
		UpdateGPUBuffersPass::Request request(vertices.data(), static_cast<uint32_t>(vertices.size()) * sizeof(vertices[0]), m_shootDebugMesh->getVertexBuffer(), 0);
		m_updateGPUBuffersPass->addRequestBeforeFrame(request);
	}

	m_shootDebugMeshRebuildRequested = false;
}
