#pragma once
#include "ResourceNonOwner.h"

class ContaminationUpdatePass;
class ParticleUpdatePass;

class RenderingPipelineInterface
{
public:
	virtual ~RenderingPipelineInterface() = default;

	virtual Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() = 0;
	virtual Wolf::ResourceNonOwner<ParticleUpdatePass> getParticleUpdatePass() = 0;
};

