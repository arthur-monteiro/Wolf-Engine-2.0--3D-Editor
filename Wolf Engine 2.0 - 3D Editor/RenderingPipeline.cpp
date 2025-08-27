#include "RenderingPipeline.h"

#include <ProfilerCommon.h>

RenderingPipeline::RenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams)
{
	m_updateGPUBuffersPass.reset(new UpdateGPUBuffersPass);
	wolfInstance->initializePass(m_updateGPUBuffersPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_preDepthPass.reset(new PreDepthPass(editorParams, true, m_updateGPUBuffersPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_preDepthPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_cascadedShadowMapsPass.reset(new CascadedShadowMapsPass);
	wolfInstance->initializePass(m_cascadedShadowMapsPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_shadowMaskPassCascadedShadowMapping.reset(new ShadowMaskPassCascadedShadowMapping(editorParams, m_preDepthPass.createNonOwnerResource(), m_cascadedShadowMapsPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_contaminationUpdatePass.reset(new ContaminationUpdatePass);
	wolfInstance->initializePass(m_contaminationUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_particleUpdatePass.reset(new ParticleUpdatePass);
	wolfInstance->initializePass(m_particleUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_thumbnailsGenerationPass.reset(new ThumbnailsGenerationPass);
	wolfInstance->initializePass(m_thumbnailsGenerationPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_rayTracedWorldDebugPass.reset(new RayTracedWorldDebugPass(m_preDepthPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_rayTracedWorldDebugPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_forwardPass.reset(new ForwardPass(editorParams, m_contaminationUpdatePass.createConstNonOwnerResource(), m_particleUpdatePass.createConstNonOwnerResource(), 
		m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<ShadowMaskPassInterface>(), m_preDepthPass.createNonOwnerResource(), m_rayTracedWorldDebugPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_drawIdsPass.reset(new DrawIdsPass(editorParams, m_preDepthPass.createNonOwnerResource(), m_forwardPass.createConstNonOwnerResource()));
	wolfInstance->initializePass(m_drawIdsPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_gpuBufferToGpuBufferCopyPass.reset(new GPUBufferToGPUBufferCopyPass(m_forwardPass.createConstNonOwnerResource(), m_drawIdsPass.createConstNonOwnerResource()));
	wolfInstance->initializePass(m_gpuBufferToGpuBufferCopyPass.createNonOwnerResource<Wolf::CommandRecordBase>());
}

void RenderingPipeline::update(Wolf::WolfEngine* wolfInstance)
{
	m_cascadedShadowMapsPass->addCamerasForThisFrame(wolfInstance->getCameraList());
	m_particleUpdatePass->updateBeforeFrame(wolfInstance->getGlobalTimer(), m_updateGPUBuffersPass.createNonOwnerResource());
	m_thumbnailsGenerationPass->addCameraForThisFrame(wolfInstance->getCameraList());
}

void RenderingPipeline::frame(Wolf::WolfEngine* wolfInstance, bool doScreenShot)
{
	PROFILE_FUNCTION

	std::vector<Wolf::ResourceNonOwner<Wolf::CommandRecordBase>> passes;
	passes.reserve(11);
	passes.push_back(m_updateGPUBuffersPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_preDepthPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_cascadedShadowMapsPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_contaminationUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_particleUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_thumbnailsGenerationPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	if (g_editorConfiguration->getEnableRayTracing())
	{
		passes.push_back(m_rayTracedWorldDebugPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}
	passes.push_back(m_forwardPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_drawIdsPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_gpuBufferToGpuBufferCopyPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	const Wolf::Semaphore* finalSemaphore = m_forwardPass->getSemaphore();
	if (!m_gpuBufferToGpuBufferCopyPass->isCurrentQueueEmpty())
		finalSemaphore = m_gpuBufferToGpuBufferCopyPass->getSemaphore();
	else if (m_drawIdsPass->isEnabledThisFrame())
		finalSemaphore = m_drawIdsPass->getSemaphore();
	wolfInstance->frame(passes, finalSemaphore);

	if (doScreenShot)
	{
		wolfInstance->waitIdle();
		m_forwardPass->saveSwapChainToFile();
	}
}

void RenderingPipeline::clear()
{
	m_updateGPUBuffersPass->clear();
}

void RenderingPipeline::setTopLevelAccelerationStructure(const Wolf::ResourceNonOwner<Wolf::TopLevelAccelerationStructure>& topLevelAccelerationStructure)
{
	m_rayTracedWorldDebugPass->setTLAS(topLevelAccelerationStructure);
}

Wolf::ResourceNonOwner<ContaminationUpdatePass> RenderingPipeline::getContaminationUpdatePass()
{
	return m_contaminationUpdatePass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<ParticleUpdatePass> RenderingPipeline::getParticleUpdatePass()
{
	return m_particleUpdatePass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<ThumbnailsGenerationPass> RenderingPipeline::getThumbnailsGenerationPass()
{
	return m_thumbnailsGenerationPass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<UpdateGPUBuffersPass> RenderingPipeline::getUpdateGPUBuffersPass()
{
	return m_updateGPUBuffersPass.createNonOwnerResource();
}

void RenderingPipeline::requestPixelId(uint32_t posX, uint32_t posY, const DrawIdsPass::PixelRequestCallback& callback) const
{
	m_drawIdsPass->requestPixelBeforeFrame(posX, posY, callback);
}
