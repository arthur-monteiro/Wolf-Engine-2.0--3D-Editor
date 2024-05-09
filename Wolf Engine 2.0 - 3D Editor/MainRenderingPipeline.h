#pragma once

#include <WolfEngine.h>

#include "ContaminationUpdatePass.h"
#include "ForwardPass.h"

class BindlessDescriptor;
class EditorParams;

class MainRenderingPipeline
{
public:
	MainRenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams);

	void update(const Wolf::WolfEngine* wolfInstance);
	void frame(Wolf::WolfEngine* wolfInstance);

	Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass();

private:
	Wolf::ResourceUniqueOwner<ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceUniqueOwner<ForwardPass> m_forwardPass;
};