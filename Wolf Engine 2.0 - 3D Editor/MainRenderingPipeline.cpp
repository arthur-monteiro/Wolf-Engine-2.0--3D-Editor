#include "MainRenderingPipeline.h"

using namespace Wolf;

MainRenderingPipeline::MainRenderingPipeline(const WolfEngine* wolfInstance, EditorParams* editorParams)
{
	m_contaminationUpdatePass.reset(new ContaminationUpdatePass);
	wolfInstance->initializePass(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());

	m_forwardPass.reset(new ForwardPass(editorParams, m_contaminationUpdatePass->getSemaphore()));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<CommandRecordBase>());
}

void MainRenderingPipeline::update(const WolfEngine* wolfInstance)
{

}

void MainRenderingPipeline::frame(WolfEngine* wolfInstance)
{
	std::vector<ResourceNonOwner<CommandRecordBase>> passes;
	passes.push_back(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());
	passes.push_back(m_forwardPass.createNonOwnerResource<CommandRecordBase>());

	wolfInstance->frame(passes, m_forwardPass->getSemaphore());
}

Wolf::ResourceNonOwner<ContaminationUpdatePass> MainRenderingPipeline::getContaminationUpdatePass()
{
	return m_contaminationUpdatePass.createNonOwnerResource();
}
