#include "MainRenderingPipeline.h"

using namespace Wolf;

MainRenderingPipeline::MainRenderingPipeline(const WolfEngine* wolfInstance, EditorParams* editorParams)
{
	m_forwardPass.reset(new ForwardPass(editorParams));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<CommandRecordBase>());
}

void MainRenderingPipeline::update(const WolfEngine* wolfInstance)
{

}

void MainRenderingPipeline::frame(WolfEngine* wolfInstance)
{
	std::vector<ResourceNonOwner<CommandRecordBase>> passes;
	passes.push_back(m_forwardPass.createNonOwnerResource<CommandRecordBase>());

	wolfInstance->frame(passes, m_forwardPass->getSemaphore());
}
