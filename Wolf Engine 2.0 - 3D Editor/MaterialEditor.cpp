#include "MaterialEditor.h"

#include "EditorParamsHelper.h"

MaterialEditor::MaterialEditor(const std::string& tab, const std::string& category)
	: m_albedoPathParam("Albedo file", tab, category, true),
      m_normalPathParam("Normal file", tab, category, true),
      m_roughnessMetalnessAOPathParam("Roughness Metalness AO file", tab, category, true)
{
}

MaterialEditor::MaterialEditor(const MaterialEditor& other)
	: m_albedoPathParam(other.m_albedoPathParam),
	  m_normalPathParam(other.m_normalPathParam),
	  m_roughnessMetalnessAOPathParam(other.m_roughnessMetalnessAOPathParam)
{
	m_materialParams = {
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessMetalnessAOPathParam
	};
}

void MaterialEditor::activateParams() const
{
	for (EditorParamInterface* param : m_materialParams)
	{
		param->activate();
	}
}

void MaterialEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const
{
	::addParamsToJSON(outJSON, m_materialParams, isLast, tabCount);
}