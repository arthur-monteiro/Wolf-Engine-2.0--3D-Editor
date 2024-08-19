#pragma once

#include <array>

#include "ComponentInterface.h"
#include "EditorConfiguration.h"
#include "EditorTypes.h"
#include "Notifier.h"

class MaterialEditor : public Notifier
{
public:
	MaterialEditor(const std::string& tab, const std::string& category, Wolf::MaterialsGPUManager::MaterialCacheInfo& materialCacheInfo);
	MaterialEditor(const MaterialEditor&) = delete;

	void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

	void activateParams();
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast);

	void getAllParams(std::vector<EditorParamInterface*>& out) const;
	void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const;

	void setMaterialId(uint32_t materialId) { m_materialId = materialId; }
	void setAlbedoPath(const std::string& albedoPath) { m_albedoPathParam = albedoPath; }
	void setNormalPath(const std::string& normalPath) { m_normalPathParam = normalPath; }
	void setRoughnessPath(const std::string& roughnessPath) { m_roughnessParam = roughnessPath; }
	void setMetalnessPath(const std::string& metalnessPath) { m_metalnessParam = metalnessPath; }
	void setAOPath(const std::string& aoPath) { m_aoParam = aoPath; }
	void setAnisoStrengthPath(const std::string& anisoStrengthPath) { m_anisoStrengthParam = anisoStrengthPath; }
	void setShadingMode(uint32_t shadingMode) { m_shadingMode = shadingMode; }

private:
	void onTextureChanged();
	void onShadingModeChanged();

	EditorParamString m_albedoPathParam;
	EditorParamString m_normalPathParam;
	EditorParamString m_roughnessParam;
	EditorParamString m_metalnessParam;
	EditorParamString m_aoParam;
	EditorParamString m_anisoStrengthParam;
	EditorParamString m_sixWaysLightmap0;
	EditorParamString m_sixWaysLightmap1;
	EditorParamBool m_enableAlpha;

	struct ShadingMode
	{
		static constexpr uint32_t GGX = 0;
		static constexpr uint32_t AnisoGGX = 1;
		static constexpr uint32_t SixWaysLighting = 2;
	};
	EditorParamEnum m_shadingMode;

	std::array<EditorParamInterface*, 6> m_shadingModeGGXParams
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessParam,
		&m_metalnessParam,
		&m_aoParam,
		&m_enableAlpha,
	};

	std::array<EditorParamInterface*, 7> m_shadingModeGGXAnisoParams
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessParam,
		&m_metalnessParam,
		&m_aoParam,
		&m_anisoStrengthParam,
		&m_enableAlpha,
	};

	std::array<EditorParamInterface*, 2> m_shadingModeSixWaysLighting
	{
		&m_sixWaysLightmap0,
		&m_sixWaysLightmap1
	};

	std::array<EditorParamInterface*, 1> m_alwaysVisibleParams
	{
		&m_shadingMode
	};

	std::array<EditorParamInterface*, 10> m_allParams =
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessParam,
		&m_metalnessParam,
		&m_aoParam,
		&m_anisoStrengthParam,
		&m_sixWaysLightmap0,
		&m_sixWaysLightmap1,
		&m_enableAlpha,
		&m_shadingMode
	};

	uint32_t m_materialId = 0;
	Wolf::MaterialsGPUManager::MaterialCacheInfo& m_materialCacheInfo;

	bool m_shadingModeChanged = false;
	bool m_textureChanged = false;
};
