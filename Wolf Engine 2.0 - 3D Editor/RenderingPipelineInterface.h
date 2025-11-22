#pragma once

#include <ResourceNonOwner.h>

class ForwardPass;
class CompositionPass;
class CascadedShadowMapsPass;
class SkyBoxManager;
class ContaminationUpdatePass;
class ComputeSkyCubeMapPass;
class ParticleUpdatePass;
class ThumbnailsGenerationPass;
class UpdateGPUBuffersPass;
class VoxelGlobalIlluminationPass;

class RenderingPipelineInterface
{
public:
	virtual ~RenderingPipelineInterface() = default;

	virtual bool hasRayTracing() const = 0;
	virtual Wolf::ResourceNonOwner<SkyBoxManager> getSkyBoxManager() = 0;
	virtual Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() = 0;
	virtual Wolf::ResourceNonOwner<ParticleUpdatePass> getParticleUpdatePass() = 0;
	virtual Wolf::ResourceNonOwner<ThumbnailsGenerationPass> getThumbnailsGenerationPass() = 0;
	virtual Wolf::ResourceNonOwner<UpdateGPUBuffersPass> getUpdateGPUBuffersPass() = 0;
	virtual Wolf::ResourceNonOwner<ComputeSkyCubeMapPass> getComputeSkyCubeMapPass() = 0;
	virtual Wolf::ResourceNonOwner<CascadedShadowMapsPass> getCascadedShadowMapsPass() = 0;
	virtual Wolf::ResourceNonOwner<CompositionPass> getCompositionPass() = 0;
	virtual Wolf::ResourceNonOwner<VoxelGlobalIlluminationPass> getVoxelGIPass() = 0;
	virtual Wolf::ResourceNonOwner<ForwardPass> getForwardPass() = 0;
};

