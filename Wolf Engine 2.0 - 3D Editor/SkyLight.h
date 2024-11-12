#pragma once

#include <glm/gtc/constants.hpp>

#include "EditorLightInterface.h"
#include "EditorTypes.h"

class SkyLight : public EditorLightInterface
{
public:
	static inline std::string ID = "skyLight";
	std::string getId() const override { return ID; }

	SkyLight();

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override {}
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	bool requiresInputs() const override { return false; }

	void addLightsToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const override;

private:
	inline static const std::string TAB = "Sky light";

	EditorParamFloat m_sunIntensity = EditorParamFloat("Intensity (cd/m2)", TAB, "Sun", 0.0f, 1000.0f);
	EditorParamVector3 m_color = EditorParamVector3("Color", TAB, "Sun", 0.0f, 1.0f);
	EditorParamVector3 m_sunDirection = EditorParamVector3("Sun direction", TAB, "Sun", -10.0f, 10.0f);

	glm::vec3 computeSunDirectionForTime(uint32_t time) const;
	void onAngleChanged();

	EditorParamFloat m_sunRisePhi = EditorParamFloat("Phi angle (radians)", TAB, "Sunrise", 0.0f, glm::pi<float>(), [this]() { onAngleChanged(); });
	EditorParamUInt m_sunRiseTime = EditorParamUInt("Time", TAB, "Sunrise", 0, 24 * 60 * 60 - 1, EditorParamUInt::ParamUIntType::TIME);

	EditorParamFloat m_solarNoonTheta = EditorParamFloat("Solar noon theta (radians)", TAB, "Solar noon", 0.0f, glm::half_pi<float>(), [this]() { onAngleChanged(); });

	EditorParamUInt m_sunSetTime = EditorParamUInt("Time", TAB, "Sunset", 0, 24 * 60 * 60 - 1, EditorParamUInt::ParamUIntType::TIME);

	EditorParamUInt m_currentTime = EditorParamUInt("Current time", TAB, "Current settings", 0, 24 * 60 * 60 - 1, EditorParamUInt::ParamUIntType::TIME);

	std::array<EditorParamInterface*, 8> m_editorParams =
	{
		&m_sunIntensity,
		&m_color,
		&m_sunDirection,

		&m_sunRisePhi,
		&m_sunRiseTime,

		&m_solarNoonTheta,

		&m_sunSetTime,

		&m_currentTime
	};

	// Debug
	void buildDebugMesh();

	bool m_debugMeshRebuildRequested = false;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_debugMesh;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_newDebugMesh; // new mesh kept here until the debug mesh is no more used
};

