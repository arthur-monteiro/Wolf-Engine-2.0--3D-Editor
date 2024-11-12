#pragma once

#include "EditorLightInterface.h"
#include "EditorTypes.h"

class PointLight : public EditorLightInterface
{
public:
	static inline std::string ID = "pointLight";
	std::string getId() const override { return ID; }

	PointLight();

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
	EditorParamFloat m_intensity = EditorParamFloat("Intensity (cd)", "Point Light", "Light", 0.0f, 1000.0f);
	EditorParamVector3 m_color = EditorParamVector3("Color", "Point Light", "Light", 0.0f, 1.0f);

	EditorParamVector3 m_position = EditorParamVector3("Position", "Point Light", "Sphere", -10.0f, 10.0f);
	EditorParamFloat m_sphereRadius = EditorParamFloat("Sphere radius", "Point Light", "Sphere", 0.0f, 10.0f);

	std::array<EditorParamInterface*, 4> m_editorParams =
	{
		&m_intensity,
		&m_color,
		&m_position,
		&m_sphereRadius
	};
};

