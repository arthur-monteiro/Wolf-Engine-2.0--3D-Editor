#include "PointLight.h"

#include "EditorParamsHelper.h"

void PointLight::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_editorParams);
}

void PointLight::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void PointLight::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

