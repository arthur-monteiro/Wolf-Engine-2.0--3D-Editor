#pragma once

#include <array>

#include "EditorTypes.h"

class MaterialEditor
{
public:
	MaterialEditor(const std::string& tab, const std::string& category);
	MaterialEditor(const MaterialEditor&);

	void activateParams() const;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const;

	std::span<EditorParamInterface*>getAllParams() { return m_materialParams; }
	std::span<EditorParamInterface* const> getAllConstParams() const { return m_materialParams; }

private:
	EditorParamString m_albedoPathParam;
	EditorParamString m_normalPathParam;
	EditorParamString m_roughnessMetalnessAOPathParam;

	std::array<EditorParamInterface*, 3> m_materialParams =
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessMetalnessAOPathParam
	};
};