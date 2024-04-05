#pragma once

#include <chrono>

#include "ComponentInterface.h"
#include "EditorTypes.h"

class PlayerComponent : public ComponentInterface
{
public:
	static inline std::string ID = "playerComponent";
	std::string getId() const override { return ID; }

	PlayerComponent();

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame() override {}
	void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	bool requiresInputs() const override { return true; }

private:
	inline static const std::string TAB = "Player component";
	EditorParamFloat m_speed = EditorParamFloat("Speed", TAB, "General", 1.0f, 10.0f);
	EditorParamUInt m_gamepadIdx = EditorParamUInt("Gamepad Idx", TAB, "General", 0, Wolf::InputHandler::MAX_GAMEPAD_COUNT);

	std::array<EditorParamInterface*, 2> m_editorParams =
	{
		&m_speed,
		&m_gamepadIdx
	};

	std::chrono::steady_clock::time_point m_lastUpdateTimePoint;
};

