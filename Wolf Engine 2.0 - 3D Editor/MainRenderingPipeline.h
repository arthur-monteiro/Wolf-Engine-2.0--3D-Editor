#pragma once

#include <WolfEngine.h>

#include "ForwardPass.h"
#include "ModelsContainer.h"

class BindlessDescriptor;
class EditorParams;

class MainRenderingPipeline
{
public:
	MainRenderingPipeline(const Wolf::WolfEngine* wolfInstance, BindlessDescriptor* bindlessDescriptor, EditorParams* editorParams);

	void update(const Wolf::WolfEngine* wolfInstance);
	void frame(Wolf::WolfEngine* wolfInstance) const;

private:
	std::unique_ptr<ForwardPass> m_forwardPass;
};