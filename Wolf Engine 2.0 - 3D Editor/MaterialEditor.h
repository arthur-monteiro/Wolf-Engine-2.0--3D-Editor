#pragma once

#include <array>

#include "EditorConfiguration.h"
#include "EditorTypes.h"
#include "Notifier.h"

class MaterialEditor : public Notifier
{
public:
	MaterialEditor(const std::string& tab, const std::string& category, Wolf::MaterialsGPUManager::MaterialCacheInfo& materialCacheInfo);
	MaterialEditor(const MaterialEditor&) = delete;

	void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

	void activateParams() const;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const;

	std::span<EditorParamInterface*> getAllParams() { return m_materialParams; }
	std::span<EditorParamInterface* const> getAllConstParams() const { return m_materialParams; }

	void setMaterialId(uint32_t materialId) { m_materialId = materialId; }
	void setAlbedoPath(const std::string& albedoPath) { m_albedoPathParam = albedoPath; }
	void setNormalPath(const std::string& normalPath) { m_normalPathParam = normalPath; }
	void setRoughnessPath(const std::string& roughnessPath) { m_roughnessParam = roughnessPath; }
	void setMetalnessPath(const std::string& metalnessPath) { m_metalnessParam = metalnessPath; }
	void setAOPath(const std::string& aoPath) { m_aoParam = aoPath; }
	void setAnisoStrengthPath(const std::string& anisoStrengthPath) { m_anisoStrengthParam = anisoStrengthPath; }
	void setShadingMode(uint32_t shadingMode) { m_shadingMode = shadingMode; }

private:
	void onAlbedoChanged();
	void onShadingModeChanged();

	EditorParamString m_albedoPathParam;
	EditorParamString m_normalPathParam;
	EditorParamString m_roughnessParam;
	EditorParamString m_metalnessParam;
	EditorParamString m_aoParam;
	EditorParamString m_anisoStrengthParam;

	EditorParamEnum m_shadingMode;

	std::array<EditorParamInterface*, 7> m_materialParams =
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessParam,
		&m_metalnessParam,
		&m_aoParam,
		&m_anisoStrengthParam,
		&m_shadingMode
	};

	uint32_t m_materialId = 0;
	Wolf::MaterialsGPUManager::MaterialCacheInfo& m_materialCacheInfo;

	bool m_shadingModeChanged = false;
	bool m_textureChanged = false;
};
