#include "PointLight.h"

#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"

PointLight::PointLight()
{
	m_sphereRadius = 0.1f;
	m_color = glm::vec3(1.0f, 1.0f, 1.0f);
}

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

void PointLight::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	debugRenderingManager.addSphere(m_position, m_sphereRadius);
}

void PointLight::addLightsToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const
{
	lightManager->addPointLightForNextFrame({ m_position, m_color, m_intensity });
}
