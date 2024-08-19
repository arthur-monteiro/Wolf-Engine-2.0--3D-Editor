#include "RenderingPipeline.h"

#include "LightManager.h"

using namespace Wolf;

RenderingPipeline::RenderingPipeline(const WolfEngine* wolfInstance, EditorParams* editorParams)
{
	m_contaminationUpdatePass.reset(new ContaminationUpdatePass);
	wolfInstance->initializePass(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());

	m_particleUpdatePass.reset(new ParticleUpdatePass);
	wolfInstance->initializePass(m_particleUpdatePass.createNonOwnerResource<CommandRecordBase>());

	m_forwardPass.reset(new ForwardPass(editorParams, m_contaminationUpdatePass->getSemaphore(), m_particleUpdatePass.createConstNonOwnerResource()));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<CommandRecordBase>());
}

void RenderingPipeline::update(const WolfEngine* wolfInstance)
{
	m_particleUpdatePass->updateBeforeFrame(wolfInstance->getGlobalTimer());
}

void RenderingPipeline::frame(WolfEngine* wolfInstance)
{
	std::vector<ResourceNonOwner<CommandRecordBase>> passes;
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
