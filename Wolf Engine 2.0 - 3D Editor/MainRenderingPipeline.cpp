#include "MainRenderingPipeline.h"

using namespace Wolf;

MainRenderingPipeline::MainRenderingPipeline(const WolfEngine* wolfInstance, EditorParams* editorParams)
{
	m_forwardPass.reset(new ForwardPass(editorParams));
	wolfInstance->initializePass(m_forwardPass.get());
}

void MainRenderingPipeline::update(const WolfEngine* wolfInstance)
{

}

void MainRenderingPipeline::frame(WolfEngine* wolfInstance) const
{
	std::vector<CommandRecordBase*> passes;
	passes.push_back(m_forwardPass.get());

	wolfInstance->frame(passes, m_forwardPass->getSemaphore());
}
