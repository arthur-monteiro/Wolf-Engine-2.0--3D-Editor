#include "RenderingPipeline.h"

#include <ProfilerCommon.h>

RenderingPipeline::RenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams, const Wolf::NullableResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager)
{
	m_skyBoxManager.reset(new SkyBoxManager);

	if (rayTracedWorldManager)
	{
		m_updateRayTracedWorldPass.reset(new UpdateRayTracedWorldPass(rayTracedWorldManager));
		wolfInstance->initializePass(m_updateRayTracedWorldPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}

	m_updateGPUBuffersPass.reset(new UpdateGPUBuffersPass);
	wolfInstance->initializePass(m_updateGPUBuffersPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	if (rayTracedWorldManager)
	{
		m_computeVertexDataPass.reset(new ComputeVertexDataPass);
		wolfInstance->initializePass(m_computeVertexDataPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}

	m_customRenderPass.reset(new CustomSceneRenderPass);
	wolfInstance->initializePass(m_customRenderPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_surfaceCoatingDataPreparationPass.reset(new SurfaceCoatingDataPreparationPass(m_customRenderPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_surfaceCoatingDataPreparationPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_preDepthPass.reset(new PreDepthPass(editorParams, true, m_updateGPUBuffersPass.createNonOwnerResource(), m_computeVertexDataPass.createNonOwnerResource(),
		m_customRenderPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_preDepthPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_cascadedShadowMapsPass.reset(new CascadedShadowMapsPass);
	wolfInstance->initializePass(m_cascadedShadowMapsPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_shadowMaskPassCascadedShadowMapping.reset(new ShadowMaskPassCascadedShadowMapping(editorParams, m_preDepthPass.createNonOwnerResource(), m_cascadedShadowMapsPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_contaminationUpdatePass.reset(new ContaminationUpdatePass);
	wolfInstance->initializePass(m_contaminationUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_particleUpdatePass.reset(new ParticleUpdatePass(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource()));
	wolfInstance->initializePass(m_particleUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_thumbnailsGenerationPass.reset(new ThumbnailsGenerationPass);
	wolfInstance->initializePass(m_thumbnailsGenerationPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_computeSkyCubeMapPass.reset(new ComputeSkyCubeMapPass(m_skyBoxManager.createNonOwnerResource()));
	wolfInstance->initializePass(m_computeSkyCubeMapPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	if (rayTracedWorldManager)
	{
		m_rayTracedShadowsPass.reset(new RayTracedShadowsPass(editorParams, m_preDepthPass.createNonOwnerResource(), m_updateRayTracedWorldPass.createNonOwnerResource(), rayTracedWorldManager));
		wolfInstance->initializePass(m_rayTracedShadowsPass.createNonOwnerResource<Wolf::CommandRecordBase>());

		m_rayTracedWorldDebugPass.reset(new RayTracedWorldDebugPass(editorParams, m_preDepthPass.createNonOwnerResource(), m_updateRayTracedWorldPass.createNonOwnerResource(), rayTracedWorldManager));
		wolfInstance->initializePass(m_rayTracedWorldDebugPass.createNonOwnerResource<Wolf::CommandRecordBase>());

		m_voxelGIPass.reset(new VoxelGlobalIlluminationPass(m_updateRayTracedWorldPass.createNonOwnerResource(), rayTracedWorldManager));
		wolfInstance->initializePass(m_voxelGIPass.createNonOwnerResource<Wolf::CommandRecordBase>());

		m_pathTracingPass.reset(new PathTracingPass(editorParams, m_preDepthPass.createNonOwnerResource(), m_computeSkyCubeMapPass.createNonOwnerResource(), rayTracedWorldManager));
		wolfInstance->initializePass(m_pathTracingPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}
	else
	{
		m_defaultGlobalIrradiance.reset(new DefaultGlobalIrradiance);
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
	Wolf::NullableResourceNonOwner<GlobalIrradiancePassInterface> globalIrradiancePassInterface;
	if (m_voxelGIPass)
	{
		globalIrradiancePassInterface = m_voxelGIPass.createNonOwnerResource<GlobalIrradiancePassInterface>();
	}
	else
	{
		globalIrradiancePassInterface = m_defaultGlobalIrradiance.createNonOwnerResource<GlobalIrradiancePassInterface>();
	}
	m_forwardPass.reset(new ForwardPass(editorParams, m_contaminationUpdatePass.createConstNonOwnerResource(), m_particleUpdatePass.createConstNonOwnerResource(), 
		m_preDepthPass.createNonOwnerResource(), rayTracedWorldDebugPass, pathTracingPass,
		m_computeSkyCubeMapPass.createNonOwnerResource(), m_skyBoxManager.createNonOwnerResource(), globalIrradiancePassInterface));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_compositionPass.reset(new CompositionPass(editorParams, m_forwardPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_compositionPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_drawIdsPass.reset(new DrawIdsPass(editorParams, m_preDepthPass.createNonOwnerResource(), m_forwardPass.createConstNonOwnerResource()));
	wolfInstance->initializePass(m_drawIdsPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	m_gpuBufferToGpuBufferCopyPass.reset(new GPUBufferToGPUBufferCopyPass(m_compositionPass.createConstNonOwnerResource(), m_drawIdsPass.createConstNonOwnerResource()));
	wolfInstance->initializePass(m_gpuBufferToGpuBufferCopyPass.createNonOwnerResource<Wolf::CommandRecordBase>());
}

void RenderingPipeline::update(Wolf::WolfEngine* wolfInstance)
{
	m_cascadedShadowMapsPass->addCamerasForThisFrame(wolfInstance->getCameraList());
	m_customRenderPass->updateBeforeFrame(wolfInstance->getCameraList());
	m_particleUpdatePass->updateBeforeFrame(wolfInstance->getGlobalTimer(), m_updateGPUBuffersPass.createNonOwnerResource());
	m_thumbnailsGenerationPass->addCameraForThisFrame(wolfInstance->getCameraList());
	m_skyBoxManager->updateBeforeFrame(wolfInstance, m_computeSkyCubeMapPass.createNonOwnerResource());
	if (m_voxelGIPass)
	{
		m_voxelGIPass->addMeshesToRenderList(wolfInstance->getRenderMeshList());
	}
	else
	{
		m_defaultGlobalIrradiance->update();
	}
}

void RenderingPipeline::frame(Wolf::WolfEngine* wolfInstance, bool doScreenShot, const GameContext& gameContext)
{
	PROFILE_FUNCTION

	std::vector<Wolf::ResourceNonOwner<Wolf::CommandRecordBase>> passes;
	passes.reserve(19);
	if (m_updateRayTracedWorldPass)
	{
		passes.push_back(m_updateRayTracedWorldPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}
	passes.push_back(m_updateGPUBuffersPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	if (m_computeVertexDataPass)
	{
		passes.push_back(m_computeVertexDataPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}
	passes.push_back(m_customRenderPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_surfaceCoatingDataPreparationPass.createNonOwnerResource<Wolf::CommandRecordBase>());
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
	if (m_voxelGIPass)
	{
		passes.push_back(m_voxelGIPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	}
	passes.push_back(m_forwardPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_compositionPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_drawIdsPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_gpuBufferToGpuBufferCopyPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	uint32_t swapChainImageIdx = wolfInstance->acquireNextSwapChainImage();
	if (swapChainImageIdx == Wolf::SwapChain::NO_IMAGE_IDX)
	{
		return;
	}

	Wolf::Semaphore* finalSemaphore = nullptr;
	if (!m_gpuBufferToGpuBufferCopyPass->isCurrentQueueEmpty())
	{
		finalSemaphore = m_gpuBufferToGpuBufferCopyPass->getSemaphore(swapChainImageIdx);
		m_gpuBufferToGpuBufferCopyPass->setIsFinalPassThisFrame();
	}
	else if (m_drawIdsPass->isEnabledThisFrame())
	{
		finalSemaphore = m_drawIdsPass->getSemaphore(swapChainImageIdx);
		m_drawIdsPass->setIsFinalPassThisFrame();
	}
	else
	{
		finalSemaphore = m_compositionPass->getSemaphore(swapChainImageIdx);
		m_compositionPass->setIsFinalPassThisFrame();
	}
	wolfInstance->frame(passes, finalSemaphore, swapChainImageIdx);

	if (doScreenShot)
	{
		wolfInstance->waitIdle();
		m_compositionPass->saveSwapChainToFile();
	}
}

void RenderingPipeline::clear()
{
	m_updateGPUBuffersPass->clear();
	m_computeSkyCubeMapPass->clear();
	m_compositionPass->clear();
}

void RenderingPipeline::setResourceManager(const Wolf::ResourceNonOwner<AssetManager>& resourceManager) const
{
	if (m_voxelGIPass)
	{
		m_voxelGIPass->setResourceManager(resourceManager);
	}
}

Wolf::ResourceNonOwner<SkyBoxManager> RenderingPipeline::getSkyBoxManager()
{
	return m_skyBoxManager.createNonOwnerResource();
}

Wolf::ResourceNonOwner<ContaminationUpdatePass> RenderingPipeline::getContaminationUpdatePass()
{
	return m_contaminationUpdatePass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<CustomSceneRenderPass> RenderingPipeline::getCustomRenderPass()
{
	return m_customRenderPass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<SurfaceCoatingDataPreparationPass> RenderingPipeline::getSurfaceCoatingDataPreparationPass()
{
	return m_surfaceCoatingDataPreparationPass.createNonOwnerResource();
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

Wolf::NullableResourceNonOwner<ComputeVertexDataPass> RenderingPipeline::getComputeVertexDataPass()
{
	if (m_computeVertexDataPass)
		return m_computeVertexDataPass.createNonOwnerResource();
	else
		return Wolf::NullableResourceNonOwner<ComputeVertexDataPass>();
}

Wolf::ResourceNonOwner<ComputeSkyCubeMapPass> RenderingPipeline::getComputeSkyCubeMapPass()
{
	return m_computeSkyCubeMapPass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<CascadedShadowMapsPass> RenderingPipeline::getCascadedShadowMapsPass()
{
	return m_cascadedShadowMapsPass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<CompositionPass> RenderingPipeline::getCompositionPass()
{
	return m_compositionPass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<VoxelGlobalIlluminationPass> RenderingPipeline::getVoxelGIPass()
{
	return m_voxelGIPass.createNonOwnerResource();
}

Wolf::ResourceNonOwner<ForwardPass> RenderingPipeline::getForwardPass()
{
	return m_forwardPass.createNonOwnerResource();
}

Wolf::Viewport RenderingPipeline::getRenderViewport() const
{
	return m_forwardPass->getRenderViewport();
}

void RenderingPipeline::requestPixelId(uint32_t posX, uint32_t posY, const DrawIdsPass::PixelRequestCallback& callback) const
{
	m_drawIdsPass->requestPixelBeforeFrame(posX, posY, callback);
}
