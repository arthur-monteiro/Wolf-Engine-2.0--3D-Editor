#include "RenderingPipeline.h"

#include "LightManager.h"

using namespace Wolf;

RenderingPipeline::RenderingPipeline(const WolfEngine* wolfInstance, EditorParams* editorParams)
{
	m_preDepthPass.reset(new PreDepthPass(editorParams, true));
	wolfInstance->initializePass(m_preDepthPass.createNonOwnerResource<CommandRecordBase>());

	m_cascadedShadowMapsPass.reset(new CascadedShadowMapsPass);
	wolfInstance->initializePass(m_cascadedShadowMapsPass.createNonOwnerResource<CommandRecordBase>());

	m_shadowMaskPassCascadedShadowMapping.reset(new ShadowMaskPassCascadedShadowMapping(editorParams, m_preDepthPass.createNonOwnerResource(), m_cascadedShadowMapsPass.createNonOwnerResource()));
	wolfInstance->initializePass(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<CommandRecordBase>());

	m_contaminationUpdatePass.reset(new ContaminationUpdatePass);
	wolfInstance->initializePass(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());

	m_particleUpdatePass.reset(new ParticleUpdatePass);
	wolfInstance->initializePass(m_particleUpdatePass.createNonOwnerResource<CommandRecordBase>());

	m_forwardPass.reset(new ForwardPass(editorParams, m_contaminationUpdatePass->getSemaphore(), m_particleUpdatePass.createConstNonOwnerResource(), 
		m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<ShadowMaskPassInterface>()));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<CommandRecordBase>());
}

void RenderingPipeline::update(WolfEngine* wolfInstance)
{
	m_cascadedShadowMapsPass->addCamerasForThisFrame(wolfInstance->getCameraList());
	m_particleUpdatePass->updateBeforeFrame(wolfInstance->getGlobalTimer());
}

void RenderingPipeline::frame(WolfEngine* wolfInstance)
{
	std::vector<ResourceNonOwner<CommandRecordBase>> passes;
	passes.push_back(m_preDepthPass.createNonOwnerResource<CommandRecordBase>());
	passes.push_back(m_cascadedShadowMapsPass.createNonOwnerResource<CommandRecordBase>());
	passes.push_back(m_shadowMaskPassCascadedShadowMapping.createNonOwnerResource<CommandRecordBase>());
	passes.push_back(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());
	passes.push_back(m_particleUpdatePass.createNonOwnerResource<CommandRecordBase>());
	passes.push_back(m_forwardPass.createNonOwnerResource<CommandRecordBase>());

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
