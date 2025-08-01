#pragma once

#include <WolfEngine.h>

#include "CascadedShadowMapsPass.h"
#include "ContaminationUpdatePass.h"
#include "DrawIdsPass.h"
#include "ForwardPass.h"
#include "GPUBufferToGPUBufferCopyPass.h"
#include "ParticleUpdatePass.h"
#include "PreDepthPass.h"
#include "RayTracedWorldDebugPass.h"
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
	RenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams);

	void update(Wolf::WolfEngine* wolfInstance);
	void frame(Wolf::WolfEngine* wolfInstance);
	void clear();

	void setTopLevelAccelerationStructure(const Wolf::ResourceNonOwner<Wolf::TopLevelAccelerationStructure>& topLevelAccelerationStructure);

	Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() override;
	Wolf::ResourceNonOwner<ParticleUpdatePass> getParticleUpdatePass() override;
	Wolf::ResourceNonOwner<ThumbnailsGenerationPass> getThumbnailsGenerationPass() override;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> getUpdateGPUBuffersPass() override;
	void requestPixelId(uint32_t posX, uint32_t posY, const DrawIdsPass::PixelRequestCallback& callback) const;

private:
	Wolf::ResourceUniqueOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
	Wolf::ResourceUniqueOwner<PreDepthPass> m_preDepthPass;
	Wolf::ResourceUniqueOwner<CascadedShadowMapsPass> m_cascadedShadowMapsPass;
	Wolf::ResourceUniqueOwner<ShadowMaskPassCascadedShadowMapping> m_shadowMaskPassCascadedShadowMapping;
	Wolf::ResourceUniqueOwner<ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceUniqueOwner<ParticleUpdatePass> m_particleUpdatePass;
	Wolf::ResourceUniqueOwner<ThumbnailsGenerationPass> m_thumbnailsGenerationPass;
	Wolf::ResourceUniqueOwner<RayTracedWorldDebugPass> m_rayTracedWorldDebugPass;
	Wolf::ResourceUniqueOwner<ForwardPass> m_forwardPass;
	Wolf::ResourceUniqueOwner<DrawIdsPass> m_drawIdsPass;
	Wolf::ResourceUniqueOwner<GPUBufferToGPUBufferCopyPass> m_gpuBufferToGpuBufferCopyPass;
};