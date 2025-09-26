#include "SkyLight.h"

#include <ImageFileLoader.h>
#include <glm/gtc/matrix_transform.hpp>

#include "ComputeSkyCubeMapPass.h"
#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"

SkyLight::SkyLight(const std::function<void(ComponentInterface*)>& requestReloadCallback, const Wolf::ResourceNonOwner<ResourceManager>& resourceManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
: m_requestReloadCallback(requestReloadCallback), m_resourceManager(resourceManager), m_renderingPipeline(renderingPipeline)
{
	m_color = glm::vec3(1.0f, 1.0f, 1.0f);
}

void SkyLight::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_alwaysVisibleParams);
	::loadParams(jsonReader, ID, m_realtimeComputeParams);
	::loadParams(jsonReader, ID, m_bakedParams);

	buildDebugMesh();
}

void SkyLight::activateParams()
{
	std::string unused;
	forAllVisibleParams([](EditorParamInterface* e, std::string&) { e->activate(); }, unused);
}

void SkyLight::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	forAllVisibleParams([tabCount](EditorParamInterface* e, std::string& inOutJSON) mutable  { e->addToJSON(inOutJSON, tabCount, false); }, outJSON);
}

void SkyLight::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	if (m_cubeMapUpdateRequested)
	{
		if (updateCubeMap())
		{
			m_cubeMapUpdateRequested = false;
		}
	}
}

void SkyLight::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	if (m_newDebugMesh)
		m_debugMesh.reset(m_newDebugMesh.release());

	const bool useNewMesh = m_debugMeshRebuildRequested || !m_debugMesh;
	if (useNewMesh)
		buildDebugMesh();

	DebugRenderingManager::LinesUBData uniformData{};
	uniformData.transform = glm::mat4(1.0f);
	debugRenderingManager.addCustomGroupOfLines(useNewMesh ? m_newDebugMesh.createNonOwnerResource() : m_debugMesh.createNonOwnerResource(), uniformData);

	debugRenderingManager.addSphere(-computeSunDirectionForTime(m_currentTime) * 500.0f, 50.0f, m_color);
}

void SkyLight::addLightsToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const
{
	lightManager->addSunLightInfoForNextFrame({ computeSunDirectionForTime(m_currentTime), 0.1f, m_color, m_sunIntensity });
}

void SkyLight::forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString)
{
	for (EditorParamInterface* editorParam : m_alwaysVisibleParams)
	{
		callback(editorParam, inOutString);
	}

	switch (static_cast<uint32_t>(m_lightType))
	{
		case LIGHT_TYPE_REALTIME_COMPUTE:
			{
				for (EditorParamInterface* editorParam : m_realtimeComputeParams)
				{
					callback(editorParam, inOutString);
				}
				break;
			}
		case LIGHT_TYPE_BAKED:
			{
				for (EditorParamInterface* editorParam : m_bakedParams)
				{
					callback(editorParam, inOutString);
				}
				break;
			}
		default:
			Wolf::Debug::sendError("Undefined light type");
			break;
	}
}

bool SkyLight::updateCubeMap()
{
	switch (static_cast<uint32_t>(m_lightType))
	{
		case LIGHT_TYPE_REALTIME_COMPUTE:
			{
				// TODO
				return true;
			}
		case LIGHT_TYPE_BAKED:
			{
				if (m_sphericalMapResourceId == ResourceManager::NO_RESOURCE || !m_resourceManager->isImageLoaded(m_sphericalMapResourceId))
					return false;

				m_renderingPipeline->getComputeSkyCubeMapPass()->setInputSphericalMap(m_resourceManager->getImage(m_sphericalMapResourceId));

				return true;
			}
		default:
			Wolf::Debug::sendError("Undefined light type");
			break;
	}

	return false;
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

void SkyLight::onSphericalMapChanged()
{
	if (static_cast<std::string>(m_sphericalMap) != "")
	{
		m_sphericalMapResourceId = m_resourceManager->addImage(m_sphericalMap, false, false, true);
		m_cubeMapUpdateRequested = true;
	}
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

		vertices.push_back({ -sunDirection * 500.0f });
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
