#pragma once
#include "ResourceNonOwner.h"

class ContaminationUpdatePass;
class ParticleUpdatePass;
class ThumbnailsGenerationPass;
class UpdateGPUBuffersPass;

class RenderingPipelineInterface
{
public:
	virtual ~RenderingPipelineInterface() = default;

	virtual Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() = 0;
	virtual Wolf::ResourceNonOwner<ParticleUpdatePass> getParticleUpdatePass() = 0;
	virtual Wolf::ResourceNonOwner<ThumbnailsGenerationPass> getThumbnailsGenerationPass() = 0;
	virtual Wolf::ResourceNonOwner<UpdateGPUBuffersPass> getUpdateGPUBuffersPass() = 0;
};

