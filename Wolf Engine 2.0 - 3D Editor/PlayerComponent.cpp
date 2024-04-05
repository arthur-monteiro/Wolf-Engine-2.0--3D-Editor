#include "PlayerComponent.h"

#include "EditorParamsHelper.h"
#include "Entity.h"

PlayerComponent::PlayerComponent()
{
	m_speed = 1.0f;
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

void PlayerComponent::updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	if (!m_entity)
		Wolf::Debug::sendCriticalError("Entity not set");

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
}
