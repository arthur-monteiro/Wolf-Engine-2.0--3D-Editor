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

	void updateBeforeFrame();

	const std::vector<PointLightInfo>& getCurrentLights() const { return m_currentPointLights; }

private:
	std::vector<PointLightInfo> m_currentPointLights;
	std::vector<PointLightInfo> m_nextFramePointLights;
};

