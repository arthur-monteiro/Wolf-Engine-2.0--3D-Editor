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

	m_forwardPass.reset(new ForwardPass(editorParams, m_contaminationUpdatePass->getSemaphore(), m_particleUpdatePass.createConstNonOwnerResource(), 
		m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<ShadowMaskPassInterface>(), m_preDepthPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<Wolf::CommandRecordBase>());
}

void RenderingPipeline::update(Wolf::WolfEngine* wolfInstance)
{
	m_cascadedShadowMapsPass->addCamerasForThisFrame(wolfInstance->getCameraList());
	m_particleUpdatePass->updateBeforeFrame(wolfInstance->getGlobalTimer());
	m_thumbnailsGenerationPass->addCameraForThisFrame(wolfInstance->getCameraList());
}

void RenderingPipeline::frame(Wolf::WolfEngine* wolfInstance)
{
	PROFILE_FUNCTION

	std::vector<Wolf::ResourceNonOwner<Wolf::CommandRecordBase>> passes;
	passes.push_back(m_updateGPUBuffersPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_preDepthPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_cascadedShadowMapsPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_contaminationUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_particleUpdatePass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_thumbnailsGenerationPass.createNonOwnerResource<Wolf::CommandRecordBase>());
	passes.push_back(m_forwardPass.createNonOwnerResource<Wolf::CommandRecordBase>());

	wolfInstance->frame(passes, m_forwardPass->getSemaphore());
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
