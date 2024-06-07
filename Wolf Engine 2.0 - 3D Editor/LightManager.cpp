#include "LightManager.h"

void LightManager::addPointLightForNextFrame(const PointLightInfo& pointLightInfo)
{
	m_nextFramePointLights.emplace_back(pointLightInfo);
}

void LightManager::updateBeforeFrame()
{
	m_currentPointLights.swap(m_nextFramePointLights);
	m_nextFramePointLights.clear();
}
