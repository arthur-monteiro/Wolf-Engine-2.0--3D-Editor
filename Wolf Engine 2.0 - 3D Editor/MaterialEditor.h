#pragma once

#include <array>

#include "EditorConfiguration.h"
#include "EditorTypes.h"
#include "Notifier.h"

class MaterialEditor : public Notifier
{
public:
	MaterialEditor(const std::string& tab, const std::string& category);
	MaterialEditor(const MaterialEditor&);

	void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

	void activateParams() const;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const;

	std::span<EditorParamInterface*>getAllParams() { return m_materialParams; }
	std::span<EditorParamInterface* const> getAllConstParams() const { return m_materialParams; }
	uint32_t getMaterialId() const { return m_materialId; }

private:
	void onTextureChanged();

	EditorParamString m_albedoPathParam;
	EditorParamString m_normalPathParam;
	EditorParamString m_roughnessParam;
	EditorParamString m_metalnessParam;
	EditorParamString m_aoParam;

	std::array<EditorParamInterface*, 5> m_materialParams =
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessParam,
		&m_metalnessParam,
		&m_aoParam
	};

	Wolf::ResourceUniqueOwner<Wolf::Image> m_albedoImage;

	uint32_t m_materialId = 0;
	bool m_updateNeeded = false;
};
