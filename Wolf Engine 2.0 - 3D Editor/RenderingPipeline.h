#pragma once

#include <WolfEngine.h>

#include "CascadedShadowMapsPass.h"
#include "CompositionPass.h"
#include "ComputeSkyCubeMapPass.h"
#include "ContaminationUpdatePass.h"
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

class LightManager;
class BindlessDescriptor;
class EditorParams;

class RenderingPipeline : public RenderingPipelineInterface
{
public:
	RenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams, const Wolf::NullableResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager);

	void update(Wolf::WolfEngine* wolfInstance);
	void frame(Wolf::WolfEngine* wolfInstance, bool doScreenShot, const GameContext& gameContext);
	void clear();

	Wolf::ResourceNonOwner<SkyBoxManager> getSkyBoxManager() override;
	Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() override;
	Wolf::ResourceNonOwner<ParticleUpdatePass> getParticleUpdatePass() override;
	Wolf::ResourceNonOwner<ThumbnailsGenerationPass> getThumbnailsGenerationPass() override;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> getUpdateGPUBuffersPass() override;
	Wolf::ResourceNonOwner<ComputeSkyCubeMapPass> getComputeSkyCubeMapPass() override;
	Wolf::ResourceNonOwner<CascadedShadowMapsPass> getCascadedShadowMapsPass() override;
	Wolf::ResourceNonOwner<CompositionPass> getCompositionPass() override;
	void requestPixelId(uint32_t posX, uint32_t posY, const DrawIdsPass::PixelRequestCallback& callback) const;

private:
	Wolf::ResourceUniqueOwner<SkyBoxManager> m_skyBoxManager;

	Wolf::ResourceUniqueOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
	Wolf::ResourceUniqueOwner<PreDepthPass> m_preDepthPass;
	Wolf::ResourceUniqueOwner<CascadedShadowMapsPass> m_cascadedShadowMapsPass;
	Wolf::ResourceUniqueOwner<ShadowMaskPassCascadedShadowMapping> m_shadowMaskPassCascadedShadowMapping;
	Wolf::ResourceUniqueOwner<RayTracedShadowsPass> m_rayTracedShadowsPass;
	Wolf::ResourceUniqueOwner<ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceUniqueOwner<ParticleUpdatePass> m_particleUpdatePass;
	Wolf::ResourceUniqueOwner<ThumbnailsGenerationPass> m_thumbnailsGenerationPass;
	Wolf::ResourceUniqueOwner<ComputeSkyCubeMapPass> m_computeSkyCubeMapPass;
	Wolf::ResourceUniqueOwner<RayTracedWorldDebugPass> m_rayTracedWorldDebugPass;
	Wolf::ResourceUniqueOwner<PathTracingPass> m_pathTracingPass;
	Wolf::ResourceUniqueOwner<ForwardPass> m_forwardPass;
	Wolf::ResourceUniqueOwner<CompositionPass> m_compositionPass;
	Wolf::ResourceUniqueOwner<DrawIdsPass> m_drawIdsPass;
	Wolf::ResourceUniqueOwner<GPUBufferToGPUBufferCopyPass> m_gpuBufferToGpuBufferCopyPass;
};