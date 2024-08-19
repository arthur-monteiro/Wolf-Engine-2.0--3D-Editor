#pragma once

#include <WolfEngine.h>

#include "ContaminationUpdatePass.h"
#include "ForwardPass.h"
#include "ParticleUpdatePass.h"
#include "RenderingPipelineInterface.h"

class LightManager;
class BindlessDescriptor;
class EditorParams;

class RenderingPipeline : public RenderingPipelineInterface
{
public:
	RenderingPipeline(const Wolf::WolfEngine* wolfInstance, EditorParams* editorParams);

	void update(const Wolf::WolfEngine* wolfInstance);
	void frame(Wolf::WolfEngine* wolfInstance);

	Wolf::ResourceNonOwner<ContaminationUpdatePass> getContaminationUpdatePass() override;
	Wolf::ResourceNonOwner<ParticleUpdatePass> getParticleUpdatePass() override;

private:
	Wolf::ResourceUniqueOwner<ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceUniqueOwner<ParticleUpdatePass> m_particleUpdatePass;
	Wolf::ResourceUniqueOwner<ForwardPass> m_forwardPass;
};