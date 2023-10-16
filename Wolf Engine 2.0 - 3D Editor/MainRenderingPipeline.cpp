#include "MainRenderingPipeline.h"

using namespace Wolf;

MainRenderingPipeline::MainRenderingPipeline(const Wolf::WolfEngine* wolfInstance, BindlessDescriptor* bindlessDescriptor, EditorParams* editorParams)
{
	m_forwardPass.reset(new ForwardPass(bindlessDescriptor, editorParams));
	wolfInstance->initializePass(m_forwardPass.get());
}

void MainRenderingPipeline::update(const Wolf::WolfEngine* wolfInstance)
{

}

void MainRenderingPipeline::frame(Wolf::WolfEngine* wolfInstance) const
{
	std::vector<CommandRecordBase*> passes;
	passes.push_back(m_forwardPass.get());

	wolfInstance->frame(passes, m_forwardPass->getSemaphore());
}
