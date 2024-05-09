#include "PlayerComponent.h"

#include <glm/gtc/constants.hpp>

#include "ContaminationEmitter.h"
#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"
#include "Entity.h"

PlayerComponent::PlayerComponent(std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback) : m_getEntityFromLoadingPathCallback(std::move(getEntityFromLoadingPathCallback))
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

void PlayerComponent::updateBeforeFrame()
{
	if (isShooting())
	{
		// Send info to contamination emitter to update the 3D texture
		if (m_contaminationEmitterEntity)
		{
			if (const Wolf::ResourceNonOwner<ContaminationEmitter> contaminationEmitterComponent = (*m_contaminationEmitterEntity)->getComponent<ContaminationEmitter>())
			{
				contaminationEmitterComponent->addShootRequest(ShootRequest{ getGunPosition(), m_shootLength, glm::radians(static_cast<float>(m_shootAngle)) });
			}
		}

		m_currentShootX = m_currentShootY = 0.0f; // reset to avoid sending the info twice
	}
}

void PlayerComponent::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	if (isShooting())
	{
		if (m_newShootDebugMesh)
			m_shootDebugMesh.reset(m_newShootDebugMesh.release());

		const bool useNewMesh = m_shootDebugMeshRebuildRequested;
		if (m_shootDebugMeshRebuildRequested)
			buildShootDebugMesh();

		DebugRenderingManager::LinesUBData uniformData{};
		uniformData.transform = glm::mat4(1.0f);
		debugRenderingManager.addCustomGroupOfLines(useNewMesh ? m_newShootDebugMesh.createNonOwnerResource() : m_shootDebugMesh.createNonOwnerResource(), uniformData);
	}
}

void PlayerComponent::updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	if (!m_entity)
		Wolf::Debug::sendCriticalError("Entity not set");

	// Move player
	float moveX, moveY;
	inputHandler->getJoystickSpeedForGamepad(static_cast<uint8_t>(m_gamepadIdx), 0, moveX, moveY, m_entity);

	const auto now = std::chrono::high_resolution_clock::now();
	const long long millisecondsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_lastUpdateTimePoint).count();
	const float secondDuration = static_cast<float>(millisecondsDuration) / 1000.0f;
	m_lastUpdateTimePoint = now;

	glm::vec3 position = m_entity->getPosition();
	position.x += moveX * secondDuration * m_speed;
	position.z += moveY * secondDuration * m_speed;
	m_entity->setPosition(position);

	// Shoot
	inputHandler->getJoystickSpeedForGamepad(static_cast<uint8_t>(m_gamepadIdx), 1, m_currentShootX, m_currentShootY, m_entity);

	if (isShooting())
	{
		m_shootDebugMeshRebuildRequested = true;
	}
}

void PlayerComponent::onContaminationEmitterChanged()
{
	m_contaminationEmitterEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_contaminationEmitterParam)));
}

glm::vec3 PlayerComponent::getGunPosition() const
{
	return m_entity->getPosition();
}

bool PlayerComponent::isShooting() const
{
	return m_currentShootX != 0.0f || m_currentShootY != 0.0f;
}

void PlayerComponent::buildShootDebugMesh()
{
	const glm::vec3 gunPosition = getGunPosition();
	const glm::vec3 shootDirection = glm::normalize(glm::vec3(m_currentShootX, 0, m_currentShootY));

	std::vector<DebugRenderingManager::LineVertex> vertices;
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

	m_newShootDebugMesh.reset(new Wolf::Mesh(vertices, indices));

	m_shootDebugMeshRebuildRequested = false;
}
