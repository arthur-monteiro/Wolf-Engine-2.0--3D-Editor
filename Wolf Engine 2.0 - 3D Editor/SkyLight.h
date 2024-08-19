#pragma once

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
	void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	bool requiresInputs() const override { return false; }

	void addLightsToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const override;

private:
	inline static const std::string TAB = "Sky light";

	EditorParamFloat m_sunIntensity = EditorParamFloat("Intensity (cd/m2)", TAB, "Sun", 0.0f, 1000.0f);
	EditorParamVector3 m_color = EditorParamVector3("Color", TAB, "Sun", 0.0f, 1.0f);
	EditorParamVector3 m_sunDirection = EditorParamVector3("Sun direction", TAB, "Sun", -10.0f, 10.0f);

	std::array<EditorParamInterface*, 3> m_editorParams =
	{
		&m_sunIntensity,
		&m_color,
		&m_sunDirection
	};
};

