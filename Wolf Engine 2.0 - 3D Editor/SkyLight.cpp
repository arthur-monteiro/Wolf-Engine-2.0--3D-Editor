#include "SkyLight.h"

#include <glm/gtc/matrix_transform.hpp>

#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"

SkyLight::SkyLight()
{
	m_color = glm::vec3(1.0f, 1.0f, 1.0f);
}

void SkyLight::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_editorParams);

	buildDebugMesh();
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

void SkyLight::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	if (m_newDebugMesh)
		m_debugMesh.reset(m_newDebugMesh.release());

	const bool useNewMesh = m_debugMeshRebuildRequested;
	if (m_debugMeshRebuildRequested)
		buildDebugMesh();

	DebugRenderingManager::LinesUBData uniformData{};
	uniformData.transform = glm::mat4(1.0f);
	debugRenderingManager.addCustomGroupOfLines(useNewMesh ? m_newDebugMesh.createNonOwnerResource() : m_debugMesh.createNonOwnerResource(), uniformData);

	debugRenderingManager.addSphere(-computeSunDirectionForTime(m_currentTime) * 500.0f, 50.0f);
}

void SkyLight::addLightsToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const
{
	lightManager->addSunLightInfoForNextFrame({ computeSunDirectionForTime(m_currentTime), m_color, m_sunIntensity });
}

glm::vec3 SkyLight::computeSunDirectionForTime(uint32_t time) const
{
	float progress = static_cast<float>(time) / (24.0f * 60.0f * 60.0f);

	float progressAngle = progress * glm::two_pi<float>() - glm::half_pi<float>();
	glm::vec3 samplePos = glm::vec3(glm::cos(progressAngle), glm::sin(progressAngle), 0.0f);

	glm::mat4 rotationAroundY = glm::rotate(glm::mat4(1.0f), -static_cast<float>(m_sunRisePhi), glm::vec3(0.0f, 1.0f, 0.0f));
	samplePos = glm::vec3(rotationAroundY * glm::vec4(samplePos, 1.0f));

	glm::vec3 sunRisePos = glm::vec3(glm::cos(m_sunRisePhi), 0.0f, glm::sin(m_sunRisePhi));
	glm::vec3 sunSetPos = glm::vec3(glm::cos(m_sunRisePhi + glm::pi<float>()), 0.0f, glm::sin(m_sunRisePhi + glm::pi<float>()));
	glm::mat4 rotationAroundRiseDawnAxis = glm::rotate(glm::mat4(1.0f), static_cast<float>(m_solarNoonTheta), glm::normalize(sunSetPos - sunRisePos));
	samplePos = glm::vec3(rotationAroundRiseDawnAxis * glm::vec4(samplePos, 1.0f));

	return -samplePos;
}

void SkyLight::onAngleChanged()
{
	m_debugMeshRebuildRequested = true;
}

void SkyLight::buildDebugMesh()
{
	std::vector<DebugRenderingManager::DebugVertex> vertices;
	std::vector<uint32_t> indices;

	static constexpr uint32_t NUM_DEBUG_POINTS = 64;
	for (uint32_t i = 0; i <= NUM_DEBUG_POINTS; ++i)
	{
		float progress = static_cast<float>(i) / static_cast<float>(NUM_DEBUG_POINTS);
		uint32_t time = static_cast<uint32_t>(progress * (24.0f * 60.0f * 60.0f));
		glm::vec3 sunDirection = computeSunDirectionForTime(time);

		vertices.push_back({ -sunDirection * 500.0f, glm::vec3(1.0f, 1.0f, 1.0f) });
		if (i > 0 && i != NUM_DEBUG_POINTS)
			vertices.push_back(vertices.back());
	}

	for (uint32_t i = 0; i < vertices.size(); ++i)
	{
		indices.push_back(i);
	}

	m_newDebugMesh.reset(new Wolf::Mesh(vertices, indices));

	m_debugMeshRebuildRequested = false;
}
