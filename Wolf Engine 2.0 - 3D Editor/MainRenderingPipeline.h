#pragma once

#include <WolfEngine.h>

#include "ForwardPass.h"

class BindlessDescriptor;
class EditorParams;

class MainRenderingPipeline
{
public:
	MainRenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams);

	void update(const Wolf::WolfEngine* wolfInstance);
	void frame(Wolf::WolfEngine* wolfInstance);

private:
	Wolf::ResourceUniqueOwner<ForwardPass> m_forwardPass;
};