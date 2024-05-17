#include "CheapRenderingPipeline.h"

using namespace Wolf;

CheapRenderingPipeline::CheapRenderingPipeline(const WolfEngine* wolfInstance, EditorParams* editorParams)
{
	m_contaminationUpdatePass.reset(new ContaminationUpdatePass);
	wolfInstance->initializePass(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());

	m_forwardPass.reset(new ForwardPassForCheapRenderingPipeline(editorParams, m_contaminationUpdatePass->getSemaphore()));
	wolfInstance->initializePass(m_forwardPass.createNonOwnerResource<CommandRecordBase>());
}

void CheapRenderingPipeline::update(const WolfEngine* wolfInstance)
{

}

void CheapRenderingPipeline::frame(WolfEngine* wolfInstance)
{
	std::vector<ResourceNonOwner<CommandRecordBase>> passes;
	passes.push_back(m_contaminationUpdatePass.createNonOwnerResource<CommandRecordBase>());
	passes.push_back(m_forwardPass.createNonOwnerResource<CommandRecordBase>());

	wolfInstance->frame(passes, m_forwardPass->getSemaphore());
}

Wolf::ResourceNonOwner<ContaminationUpdatePass> CheapRenderingPipeline::getContaminationUpdatePass()
{
	return m_contaminationUpdatePass.createNonOwnerResource();
}
