#pragma once

#include <chrono>

#include "ComponentInterface.h"
#include "EditorTypes.h"

class PlayerComponent : public ComponentInterface
{
public:
	static inline std::string ID = "playerComponent";
	std::string getId() const override { return ID; }

	PlayerComponent(std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	bool requiresInputs() const override { return true; }

private:
	inline static const std::string TAB = "Player component";
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	EditorParamFloat m_speed = EditorParamFloat("Speed", TAB, "Movement", 1.0f, 10.0f);

	EditorParamFloat m_shootLength = EditorParamFloat("Shoot length", TAB, "Shoot", 1.0f, 30.0f);
	EditorParamFloat m_shootAngle = EditorParamFloat("Shoot angle in degree", TAB, "Shoot", 1.0f, 30.0f);
	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_contaminationEmitterEntity;
	void onContaminationEmitterChanged();
	EditorParamString m_contaminationEmitterParam = EditorParamString("Contamination Emitter", TAB, "Shoot", [this]() { onContaminationEmitterChanged(); }, EditorParamString::ParamStringType::ENTITY);

	EditorParamUInt m_gamepadIdx = EditorParamUInt("Gamepad Idx", TAB, "General", 0, Wolf::InputHandler::MAX_GAMEPAD_COUNT);

	std::array<EditorParamInterface*, 5> m_editorParams =
	{
		&m_speed,
		&m_gamepadIdx,
		&m_shootLength,
		&m_shootAngle,
		&m_contaminationEmitterParam
	};

	std::chrono::steady_clock::time_point m_lastUpdateTimePoint;

	// Shoot
	float m_currentShootX = 0.0f, m_currentShootY = 0.0f;

	glm::vec3 getGunPosition() const;
	bool isShooting() const;

	// Debug
	void buildShootDebugMesh();

	bool m_shootDebugMeshRebuildRequested = false;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_shootDebugMesh;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_newShootDebugMesh; // new mesh kept here until the debug mesh is no more used
};

