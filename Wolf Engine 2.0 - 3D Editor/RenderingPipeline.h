#pragma once

#include <WolfEngine.h>

#include "CascadedShadowMapsPass.h"
#include "CompositionPass.h"
#include "ComputeSkyCubeMapPass.h"
#include "ComputeVertexDataPass.h"
#include "ContaminationUpdatePass.h"
#include "DefaultGlobalIrradiance.h"
#include "DrawIdsPass.h"
#include "ForwardPass.h"
#include "GPUBufferToGPUBufferCopyPass.h"
#include "ParticleUpdatePass.h"
#include "PathTracingPass.h"
#include "PreDepthPass.h"
#include "RayTracedShadowsPass.h"
#include "RayTracedWorldDebugPass.h"
#include "RayTracedWorldManager.h"
#include "RenderingPipelineInterface.h"
#include "ShadowMaskPassCascadedShadowMapping.h"
#include "ThumbnailsGenerationPass.h"
#include "UpdateGPUBuffersPass.h"
#include "UpdateRayTracedWorldPass.h"
#include "VoxelGlobalIlluminationPass.h"

class LightManager;
class BindlessDescriptor;
class EditorParams;
class AssetManager;

class RenderingPipeline : public RenderingPipelineInterface
{
public:
	RenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams, const Wolf::NullableResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager);

	void update(Wolf::WolfEngine* wolfInstance);
	void frame(Wolf::WolfEngine* wolfInstance, bool doScreenShot, const GameContext& gameContext);
	void clear();

	void setResourceManager(const Wolf::ResourceNonOwner<AssetManager>& resourceManager) const;

	bool hasRayTracing() const override { return static_cast<bool>(m_rayTracedWorldDebugPass); };
	Wolf::ResourceNonOwner<SkyBoxManager> getSkyBoxManager() override;
	Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() override;
	Wolf::ResourceNonOwner<ParticleUpdatePass> getParticleUpdatePass() override;
	Wolf::ResourceNonOwner<ThumbnailsGenerationPass> getThumbnailsGenerationPass() override;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> getUpdateGPUBuffersPass() override;
	Wolf::NullableResourceNonOwner<ComputeVertexDataPass> getComputeVertexDataPass() override;
	Wolf::ResourceNonOwner<ComputeSkyCubeMapPass> getComputeSkyCubeMapPass() override;
	Wolf::ResourceNonOwner<CascadedShadowMapsPass> getCascadedShadowMapsPass() override;
	Wolf::ResourceNonOwner<CompositionPass> getCompositionPass() override;
	Wolf::ResourceNonOwner<VoxelGlobalIlluminationPass> getVoxelGIPass() override;
	Wolf::ResourceNonOwner<ForwardPass> getForwardPass() override;
	void requestPixelId(uint32_t posX, uint32_t posY, const DrawIdsPass::PixelRequestCallback& callback) const;

private:
	Wolf::ResourceUniqueOwner<SkyBoxManager> m_skyBoxManager;

	Wolf::ResourceUniqueOwner<UpdateRayTracedWorldPass> m_updateRayTracedWorldPass;
	Wolf::ResourceUniqueOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
	Wolf::ResourceUniqueOwner<ComputeVertexDataPass> m_computeVertexDataPass;
	Wolf::ResourceUniqueOwner<PreDepthPass> m_preDepthPass;
	Wolf::ResourceUniqueOwner<CascadedShadowMapsPass> m_cascadedShadowMapsPass;
	Wolf::ResourceUniqueOwner<ShadowMaskPassCascadedShadowMapping> m_shadowMaskPassCascadedShadowMapping;
	Wolf::ResourceUniqueOwner<RayTracedShadowsPass> m_rayTracedShadowsPass;
	Wolf::ResourceUniqueOwner<ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceUniqueOwner<ParticleUpdatePass> m_particleUpdatePass;
	Wolf::ResourceUniqueOwner<ThumbnailsGenerationPass> m_thumbnailsGenerationPass;
	Wolf::ResourceUniqueOwner<ComputeSkyCubeMapPass> m_computeSkyCubeMapPass;
	Wolf::ResourceUniqueOwner<RayTracedWorldDebugPass> m_rayTracedWorldDebugPass;
	Wolf::ResourceUniqueOwner<DefaultGlobalIrradiance> m_defaultGlobalIrradiance;
	Wolf::ResourceUniqueOwner<VoxelGlobalIlluminationPass> m_voxelGIPass;
	Wolf::ResourceUniqueOwner<PathTracingPass> m_pathTracingPass;
	Wolf::ResourceUniqueOwner<ForwardPass> m_forwardPass;
	Wolf::ResourceUniqueOwner<CompositionPass> m_compositionPass;
	Wolf::ResourceUniqueOwner<DrawIdsPass> m_drawIdsPass;
	Wolf::ResourceUniqueOwner<GPUBufferToGPUBufferCopyPass> m_gpuBufferToGpuBufferCopyPass;
};