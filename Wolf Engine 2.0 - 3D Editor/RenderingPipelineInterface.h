#pragma once

#include <ResourceNonOwner.h>
#include <Structs.h>

class CustomSceneRenderPass;
class CascadedShadowMapsPass;
class CompositionPass;
class ComputeSkyCubeMapPass;
class ComputeVertexDataPass;
class ContaminationUpdatePass;
class ForwardPass;
class ParticleUpdatePass;
class SkyBoxManager;
class SurfaceCoatingDataPreparationPass;
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
	virtual Wolf::ResourceNonOwner<CustomSceneRenderPass> getCustomRenderPass() = 0;
	virtual Wolf::ResourceNonOwner<SurfaceCoatingDataPreparationPass> getSurfaceCoatingDataPreparationPass() = 0;
	virtual Wolf::ResourceNonOwner<ParticleUpdatePass> getParticleUpdatePass() = 0;
	virtual Wolf::ResourceNonOwner<ThumbnailsGenerationPass> getThumbnailsGenerationPass() = 0;
	virtual Wolf::ResourceNonOwner<UpdateGPUBuffersPass> getUpdateGPUBuffersPass() = 0;
	virtual Wolf::NullableResourceNonOwner<ComputeVertexDataPass> getComputeVertexDataPass() = 0;
	virtual Wolf::ResourceNonOwner<ComputeSkyCubeMapPass> getComputeSkyCubeMapPass() = 0;
	virtual Wolf::ResourceNonOwner<CascadedShadowMapsPass> getCascadedShadowMapsPass() = 0;
	virtual Wolf::ResourceNonOwner<CompositionPass> getCompositionPass() = 0;
	virtual Wolf::ResourceNonOwner<VoxelGlobalIlluminationPass> getVoxelGIPass() = 0;
	virtual Wolf::ResourceNonOwner<ForwardPass> getForwardPass() = 0;
	[[nodiscard]] virtual Wolf::Viewport getRenderViewport() const = 0;
};

