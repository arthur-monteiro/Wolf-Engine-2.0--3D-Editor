#pragma once

#include <glm/glm.hpp>
#include <vector>

class LightManager
{
public:
	struct PointLightInfo
	{
		glm::vec3 worldPos;
		glm::vec3 color;
		float intensity;
	};
	void addPointLightForNextFrame(const PointLightInfo& pointLightInfo);

	struct SunLightInfo
	{
		glm::vec3 direction;
		glm::vec3 color;
		float intensity;
	};
	void addSunLightInfoForNextFrame(const SunLightInfo& sunLightInfo);

	void updateBeforeFrame();

	const std::vector<PointLightInfo>& getCurrentPointLights() const { return m_currentPointLights; }
	const std::vector<SunLightInfo>& getCurrentSunLights() const { return m_currentSunLights; }

private:
	std::vector<PointLightInfo> m_currentPointLights;
	std::vector<PointLightInfo> m_nextFramePointLights;

	std::vector<SunLightInfo> m_currentSunLights;
	std::vector<SunLightInfo> m_nextFrameSunLights;
};

