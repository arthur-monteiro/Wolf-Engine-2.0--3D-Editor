#pragma once
#include "ResourceNonOwner.h"

class ContaminationUpdatePass;

class RenderingPipelineInterface
{
public:
	virtual ~RenderingPipelineInterface() = default;

	virtual Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() = 0;
};

