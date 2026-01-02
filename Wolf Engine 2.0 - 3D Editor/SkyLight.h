#pragma once

#include <glm/gtc/constants.hpp>

#include "EditorLightInterface.h"
#include "EditorTypes.h"
#include "AssetManager.h"

class SkyLight : public EditorLightInterface
{
public:
	static inline std::string ID = "skyLight";
	std::string getId() const override { return ID; }

	SkyLight(const std::function<void(ComponentInterface*)>& requestReloadCallback, const Wolf::ResourceNonOwner<AssetManager>& resourceManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	void saveCustom() const override {}

	void addLightsToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const override;

private:
	glm::vec3 getSunDirection() const;
	float getSunIntensity() const;
	glm::vec3 getSunColor() const;

	void forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString);
	bool updateCubeMap();

	inline static const std::string TAB = "Sky light";
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	Wolf::ResourceNonOwner<AssetManager> m_resourceManager;
	Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;

	std::vector<std::string> LIGHT_TYPE_STRING_LIST = { "Realtime", "Baked" };
	static constexpr uint32_t LIGHT_TYPE_REALTIME_COMPUTE = 0;
	static constexpr uint32_t LIGHT_TYPE_BAKED = 1;
	EditorParamEnum m_lightType = EditorParamEnum(LIGHT_TYPE_STRING_LIST, "Light model type", TAB, "General", [this]() { m_requestReloadCallback(this); });

	/* Realtime compute */
	EditorParamFloat m_sunIntensity = EditorParamFloat("Intensity (cd/m2)", TAB, "Sun", 0.0f, 1000.0f);
	EditorParamVector3 m_color = EditorParamVector3("Color", TAB, "Sun", 0.0f, 1.0f);

	glm::vec3 computeSunDirectionForTime(uint32_t time) const;
	void onAngleChanged();

	EditorParamFloat m_sunRisePhi = EditorParamFloat("Phi angle (radians)", TAB, "Sunrise", 0.0f, glm::pi<float>(), [this]() { onAngleChanged(); });
	EditorParamUInt m_sunRiseTime = EditorParamUInt("Time", TAB, "Sunrise", 0, 24 * 60 * 60 - 1, EditorParamUInt::ParamUIntType::TIME);

	EditorParamFloat m_solarNoonTheta = EditorParamFloat("Solar noon theta (radians)", TAB, "Solar noon", 0.0f, glm::half_pi<float>(), [this]() { onAngleChanged(); });

	EditorParamUInt m_sunSetTime = EditorParamUInt("Time", TAB, "Sunset", 0, 24 * 60 * 60 - 1, EditorParamUInt::ParamUIntType::TIME);

	EditorParamUInt m_currentTime = EditorParamUInt("Current time", TAB, "Current settings", 0, 24 * 60 * 60 - 1, EditorParamUInt::ParamUIntType::TIME);

	/* Baked */
	void onSphericalMapChanged();
	AssetId m_sphericalMapAssetId = NO_ASSET;
	EditorParamString m_sphericalMap = EditorParamString("Spherical map", TAB, "Sky", [this] { onSphericalMapChanged(); }, EditorParamString::ParamStringType::FILE_IMG);
	glm::vec3 m_sunDirectionFromSphericalMap;
	float m_sunIntensityFromSphericalMap;
	glm::vec3 m_sunColorFromSphericalMap;

	std::array<EditorParamInterface*, 1> m_alwaysVisibleParams =
	{
		&m_lightType
	};

	std::array<EditorParamInterface*, 7> m_realtimeComputeParams =
	{
		&m_sunIntensity,
		&m_color,

		&m_sunRisePhi,
		&m_sunRiseTime,

		&m_solarNoonTheta,

		&m_sunSetTime,

		&m_currentTime
	};

	std::array<EditorParamInterface*, 1> m_bakedParams =
	{
		&m_sphericalMap
	};

	bool m_cubeMapUpdateRequested = true;

	// Debug
	void buildDebugMesh();

	bool m_debugMeshRebuildRequested = false;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_debugMesh;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_newDebugMesh; // new mesh kept here until the debug mesh is no more used
};

