#include "RenderingPipeline.h"

#include "LightManager.h"

using namespace Wolf;

RenderingPipeline::RenderingPipeline(const WolfEngine* wolfInstance, EditorParams* editorParams)
{
	m_contaminationUpdatePass.reset(new ContaminationUpdatePass);
	wolfInstance->initializePass(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());

	m_forwardPass.reset(new ForwardPass(editorParams, m_contaminationUpdatePass->getSemaphore()));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<CommandRecordBase>());
}

void RenderingPipeline::update(const WolfEngine* wolfInstance, const Wolf::ResourceNonOwner<LightManager>& lightManager)
{
	m_forwardPass->updateLights(lightManager);
}

void RenderingPipeline::frame(WolfEngine* wolfInstance)
{
	std::vector<ResourceNonOwner<CommandRecordBase>> passes;
	passes.push_back(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());
	passes.push_back(m_forwardPass.createNonOwnerResource<CommandRecordBase>());

	wolfInstance->frame(passes, m_forwardPass->getSemaphore());
}

Wolf::ResourceNonOwner<ContaminationUpdatePass> RenderingPipeline::getContaminationUpdatePass()
{
	return m_contaminationUpdatePass.createNonOwnerResource();
}
