#include "SkyLight.h"

#include "EditorParamsHelper.h"

SkyLight::SkyLight()
{
	m_color = glm::vec3(1.0f, 1.0f, 1.0f);
}

void SkyLight::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_editorParams);
}

void SkyLight::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void SkyLight::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void SkyLight::addLightsToLightManager(const Wolf::ResourceNonOwner<LightManager>& lightManager) const
{
	lightManager->addSunLightInfoForNextFrame({ m_sunDirection, m_color, m_sunIntensity });
}
