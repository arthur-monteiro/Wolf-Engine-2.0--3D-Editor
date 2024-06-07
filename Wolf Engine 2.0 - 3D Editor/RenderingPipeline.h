#pragma once

#include <WolfEngine.h>

#include "ContaminationUpdatePass.h"
#include "ForwardPass.h"
#include "RenderingPipelineInterface.h"

class LightManager;
class BindlessDescriptor;
class EditorParams;

class RenderingPipeline : public RenderingPipelineInterface
{
public:
	RenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams);

	void update(const Wolf::WolfEngine* wolfInstance, const Wolf::ResourceNonOwner<LightManager>& lightManager);
	void frame(Wolf::WolfEngine* wolfInstance);

	Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() override;

private:
	Wolf::ResourceUniqueOwner<ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceUniqueOwner<ForwardPass> m_forwardPass;
	
};