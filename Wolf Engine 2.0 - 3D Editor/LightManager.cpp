#include "LightManager.h"

void LightManager::addPointLightForNextFrame(const PointLightInfo& pointLightInfo)
{
	m_nextFramePointLights.emplace_back(pointLightInfo);
}

void LightManager::addSunLightInfoForNextFrame(const SunLightInfo& sunLightInfo)
{
	m_nextFrameSunLights.emplace_back(sunLightInfo);
}

void LightManager::updateBeforeFrame()
{
	m_currentPointLights.swap(m_nextFramePointLights);
	m_nextFramePointLights.clear();

	m_currentSunLights.swap(m_nextFrameSunLights);
	m_nextFrameSunLights.clear();
}
