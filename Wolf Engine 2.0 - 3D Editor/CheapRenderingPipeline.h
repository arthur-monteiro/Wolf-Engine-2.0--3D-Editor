#pragma once

#include <WolfEngine.h>

#include "ContaminationUpdatePass.h"
#include "ForwardPassForCheapRenderingPipeline.h"
#include "RenderingPipelineInterface.h"

class BindlessDescriptor;
class EditorParams;

class CheapRenderingPipeline : public RenderingPipelineInterface
{
public:
	CheapRenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams);

	void update(const Wolf::WolfEngine* wolfInstance);
	void frame(Wolf::WolfEngine* wolfInstance);

	Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() override;

private:
	Wolf::ResourceUniqueOwner<ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceUniqueOwner<ForwardPassForCheapRenderingPipeline> m_forwardPass;
};