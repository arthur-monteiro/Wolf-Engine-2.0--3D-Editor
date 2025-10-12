#include "RenderingPipeline.h"

#include <ProfilerCommon.h>

RenderingPipeline::RenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams, const Wolf::NullableResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager)
{
	m_skyBoxManager.reset(new SkyBoxManager);

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

	m_computeSkyCubeMapPass.reset(new ComputeSkyCubeMapPass(m_skyBoxManager.createNonOwnerResource()));
	wolfInstance->initializePass(m_computeSkyCubeMapPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	if (rayTracedWorldManager)
	{
		m_rayTracedShadowsPass.reset(new RayTracedShadowsPass(editorParams, m_preDepthPass.createNonOwnerResource(), rayTracedWorldManager));
		wolfInstance->initializePass(m_rayTracedShadowsPass.createNonOwnerResource<Wolf::CommandRecordBase>());

		m_rayTracedWorldDebugPass.reset(new RayTracedWorldDebugPass(editorParams, m_preDepthPass.createNonOwnerResource(), rayTracedWorldManager));
		wolfInstance->initializePass(m_rayTracedWorldDebugPass.createNonOwnerResource<Wolf::CommandRecordBase>());

		m_pathTracingPass.reset(new PathTracingPass(editorParams, m_preDepthPass.createNonOwnerResource(), m_computeSkyCubeMapPass.createNonOwnerResource(), rayTracedWorldManager));
		wolfInstance->initializePass(m_pathTracingPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}

	Wolf::NullableResourceNonOwner<RayTracedWorldDebugPass> rayTracedWorldDebugPass;
	if (m_rayTracedWorldDebugPass)
	{
		rayTracedWorldDebugPass = m_rayTracedWorldDebugPass.createNonOwnerResource();
	}
	Wolf::NullableResourceNonOwner<PathTracingPass> pathTracingPass;
	if (m_pathTracingPass)
	{
		pathTracingPass = m_pathTracingPass.createNonOwnerResource();
	}
	m_forwardPass.reset(new ForwardPass(editorParams, m_contaminationUpdatePass.createConstNonOwnerResource(), m_particleUpdatePass.createConstNonOwnerResource(), 
		m_preDepthPass.createNonOwnerResource(), rayTracedWorldDebugPass, pathTracingPass,
		m_computeSkyCubeMapPass.createNonOwnerResource(), m_skyBoxManager.createNonOwnerResource()));
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
	m_skyBoxManager->updateBeforeFrame(wolfInstance, m_computeSkyCubeMapPass.createNonOwnerResource());
}

void RenderingPipeline::frame(Wolf::WolfEngine* wolfInstance, bool doScreenShot, const GameContext& gameContext)
{
	PROFILE_FUNCTION

	std::vector<Wolf::ResourceNonOwner<Wolf::CommandRecordBase>> passes;
	passes.reserve(11);
	passes.push_back(m_updateGPUBuffersPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_preDepthPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	if (gameContext.shadowTechnique == GameContext::ShadowTechnique::CSM)
	{
		passes.push_back(m_cascadedShadowMapsPass.createNonOwnerResource<Wolf::CommandRecordBase>());
		passes.push_back(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<Wolf::CommandRecordBase>());

		m_forwardPass->setShadowMaskPass(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<ShadowMaskPassInterface>(), wolfInstance->getGraphicAPIManager());
	}
	else if (gameContext.shadowTechnique == GameContext::ShadowTechnique::RAY_TRACED)
	{
		passes.push_back(m_rayTracedShadowsPass.createNonOwnerResource<Wolf::CommandRecordBase>());

		m_forwardPass->setShadowMaskPass(m_rayTracedShadowsPass.createNonOwnerResource<ShadowMaskPassInterface>(), wolfInstance->getGraphicAPIManager());
	}
	else
	{
		Wolf::Debug::sendError("Unknown shadow technique");
	}

	passes.push_back(m_contaminationUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_particleUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_thumbnailsGenerationPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_computeSkyCubeMapPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	if (m_rayTracedWorldDebugPass)
	{
		passes.push_back(m_rayTracedWorldDebugPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}
	if (m_pathTracingPass)
	{
		passes.push_back(m_pathTracingPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}
	passes.push_back(m_forwardPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_drawIdsPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_gpuBufferToGpuBufferCopyPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	uint32_t swapChainImageIdx = wolfInstance->acquireNextSwapChainImage();

	Wolf::Semaphore* finalSemaphore = nullptr;
	if (!m_gpuBufferToGpuBufferCopyPass->isCurrentQueueEmpty())
	{
		finalSemaphore = m_gpuBufferToGpuBufferCopyPass->getSemaphore(swapChainImageIdx);
	}
	else if (m_drawIdsPass->isEnabledThisFrame())
	{
		finalSemaphore = m_drawIdsPass->getSemaphore(swapChainImageIdx);
	}
	else
	{
		finalSemaphore = m_forwardPass->getSemaphore(swapChainImageIdx);
	}
	wolfInstance->frame(passes, finalSemaphore, swapChainImageIdx);

	if (doScreenShot)
	{
		wolfInstance->waitIdle();
		m_forwardPass->saveSwapChainToFile();
	}
}

void RenderingPipeline::clear()
{
	m_updateGPUBuffersPass->clear();
	m_computeSkyCubeMapPass->clear();
}

Wolf::ResourceNonOwner<SkyBoxManager> RenderingPipeline::getSkyBoxManager()
{
	return m_skyBoxManager.createNonOwnerResource();
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

Wolf::ResourceNonOwner<ComputeSkyCubeMapPass> RenderingPipeline::getComputeSkyCubeMapPass()
{
	return m_computeSkyCubeMapPass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<CascadedShadowMapsPass> RenderingPipeline::getCascadedShadowMapsPass()
{
	return m_cascadedShadowMapsPass.createNonOwnerResource();
}

void RenderingPipeline::requestPixelId(uint32_t posX, uint32_t posY, const DrawIdsPass::PixelRequestCallback& callback) const
{
	m_drawIdsPass->requestPixelBeforeFrame(posX, posY, callback);
}
