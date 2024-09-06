#include "CascadedShadowMapsPass.h"

#include <RenderMeshList.h>

#include "CommonLayouts.h"
#include "DebugMarker.h"
#include "GameContext.h"
#include "LightManager.h"

CascadeDepthPass::CascadeDepthPass(const Wolf::InitializationContext& context, uint32_t width, uint32_t height,
                                   const Wolf::CommandBuffer* commandBuffer, uint32_t cameraIdx)
{
	m_commandBuffer = commandBuffer;
	m_width = width;
	m_height = height;

	DepthPassBase::initializeResources(context);

	m_camera.reset(new Wolf::OrthographicCamera(glm::vec3(0.0f), 0.0f, 50.0f, glm::vec3(0.0f)));
	m_cameraIdx = cameraIdx;
}

void CascadeDepthPass::setCameraInfos(const glm::vec3& center, float radius, const glm::vec3& direction) const
{
	m_camera->setCenter(center);
	m_camera->setDirection(direction);
	m_camera->setRadius(radius);
}

void CascadeDepthPass::recordDraws(const Wolf::RecordContext& context)
{
	const Wolf::CommandBuffer& commandBuffer = getCommandBuffer(context);
	context.renderMeshList->draw(context, commandBuffer, m_renderPass.get(), CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP, m_cameraIdx, {});
}

const Wolf::CommandBuffer& CascadeDepthPass::getCommandBuffer(const Wolf::RecordContext& context)
{
	return *m_commandBuffer;
}

void CascadedShadowMapsPass::initializeResources(const Wolf::InitializationContext& context)
{
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false));
	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

	for (uint32_t i = 0; i < m_cascadeDepthPasses.size(); ++i)
	{
		m_cascadeDepthPasses[i].reset(new CascadeDepthPass(context, m_cascadeTextureSize[i], m_cascadeTextureSize[i], m_commandBuffer.get(), CommonCameraIndices::CAMERA_IDX_SHADOW_CASCADE_0 + i));
	}

	const float near = 0.1f; // context.camera->getNear();
	const float far = 50.0f; // context.camera->getFar(); // we don't render shadows on all the range
	uint32_t cascadeIdx = 0;
	for (float i(1.0f / CASCADE_COUNT); i <= 1.0f; i += 1.0f / CASCADE_COUNT)
	{
		float d_uni = glm::mix(near, far, i);
		float d_log = near * glm::pow((far / near), i);

		m_cascadeSplits[cascadeIdx] = (glm::mix(d_uni, d_log, 0.5f));
		cascadeIdx++;
	}
}

void CascadedShadowMapsPass::resize(const Wolf::InitializationContext& context)
{
	// Nothing to do
}

void CascadedShadowMapsPass::record(const Wolf::RecordContext& context)
{
	const Wolf::CameraInterface* camera = context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN);

	uint32_t sunLightCount = context.lightManager->getSunLightCount();
	if (sunLightCount == 0)
	{
		m_wasEnabledThisFrame = false;
		return;
	}
	m_wasEnabledThisFrame = true;
	if (sunLightCount > 1)
		Wolf::Debug::sendError("CSM doesn't support more than 1 sun");

	glm::vec3 sunDirection = context.lightManager->getSunLightInfo(0).direction;

	/* Update */
	float lastSplitDist = camera->getNear();
	for (int cascade(0); cascade < CASCADE_COUNT; ++cascade)
	{
		const float startCascade = lastSplitDist;
		const float endCascade = m_cascadeSplits[cascade];

		float radius = (endCascade - startCascade) / 2.0f;

		const float ar = static_cast<float>(context.swapchainImage[0].getExtent().height) / static_cast<float>(context.swapchainImage[0].getExtent().width);
		const float cosHalfHFOV = glm::cos(camera->getFOV() * (1.0f / ar) / 2.0f);
		const float b = endCascade / cosHalfHFOV;
		radius = glm::sqrt(b * b + (startCascade + radius) * (startCascade + radius) - 2.0f * b * startCascade * cosHalfHFOV) * 0.75f;

		const float texelPerUnit = static_cast<float>(m_cascadeTextureSize[cascade]) / (radius * 2.0f);
		glm::mat4 scaleMat = scale(glm::mat4(1.0f), glm::vec3(texelPerUnit));
		glm::mat4 lookAt = scaleMat * glm::lookAt(glm::vec3(0.0f), -sunDirection, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lookAtInv = inverse(lookAt);

		glm::vec3 frustumCenter = camera->getPosition() + (camera->getOrientation() * startCascade + camera->getOrientation() * endCascade) / 2.0f;
		frustumCenter = lookAt * glm::vec4(frustumCenter, 1.0f);
		frustumCenter.x = floor(frustumCenter.x);
		frustumCenter.y = floor(frustumCenter.y);
		frustumCenter = lookAtInv * glm::vec4(frustumCenter, 1.0f);

		m_cascadeDepthPasses[cascade]->setCameraInfos(frustumCenter, radius, sunDirection);

		lastSplitDist += m_cascadeSplits[cascade];
	}

	/* Command buffer record */
	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Cascade shadow maps");

	for (uint32_t i = 0, end = static_cast<uint32_t>(m_cascadeDepthPasses.size()); i < end; ++i)
	{
		constexpr Wolf::ColorFloat color = { 0.4f, 0.4f, 0.4f, 1.0f };
		Wolf::DebugMarker::insert(m_commandBuffer.get(), color, "Cascade " + std::to_string(i));
		m_cascadeDepthPasses[i]->record(context);
	}

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void CascadedShadowMapsPass::submit(const Wolf::SubmitContext& context)
{
	const std::vector<const Wolf::Semaphore*> waitSemaphores{ };
	const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };
	m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);
}

void CascadedShadowMapsPass::addCamerasForThisFrame(Wolf::CameraList& cameraList) const
{
	for (const std::unique_ptr<CascadeDepthPass>& cascade : m_cascadeDepthPasses)
	{
		cascade->addCameraForThisFrame(cameraList);
	}
}
