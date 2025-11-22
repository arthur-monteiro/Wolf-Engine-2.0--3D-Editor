#pragma once

#include <CommandRecordBase.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <FrameBuffer.h>
#include <Mesh.h>
#include <Pipeline.h>
#include <RenderPass.h>
#include <Sampler.h>
#include <ShaderParser.h>
#include <UniformBuffer.h>

#include "EditorParams.h"
#include "GlobalIrradiancePassInterface.h"
#include "ParticleUpdatePass.h"
#include "PathTracingPass.h"
#include "PreDepthPass.h"
#include "RayTracedWorldDebugPass.h"
#include "ShadowMaskPassInterface.h"

class ForwardPass : public Wolf::CommandRecordBase
{
public:
	ForwardPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<const ContaminationUpdatePass>& contaminationUpdatePass, const Wolf::ResourceNonOwner<const ParticleUpdatePass>& particlesUpdatePass,
		const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::NullableResourceNonOwner<RayTracedWorldDebugPass>& rayTracedWorldDebugPass,
		const Wolf::NullableResourceNonOwner<PathTracingPass>& pathTracingPass, const Wolf::ResourceNonOwner<ComputeSkyCubeMapPass>& computeSkyCubeMapPass, const Wolf::ResourceNonOwner<SkyBoxManager>& skyBoxManager,
		const Wolf::NullableResourceNonOwner<GlobalIrradiancePassInterface>& globalIrradiancePass)
	: m_editorParams(editorParams), m_contaminationUpdatePass(contaminationUpdatePass), m_particlesUpdatePass(particlesUpdatePass), m_preDepthPass(preDepthPass),
	  m_rayTracedWorldDebugPass(rayTracedWorldDebugPass), m_pathTracingPass(pathTracingPass), m_computeSkyCubeMapPass(computeSkyCubeMapPass), m_skyBoxManager(skyBoxManager), m_globalIrradiancePass(globalIrradiancePass)
	{}

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	Wolf::Image& getOutput() { return *m_outputImage; }

	void setShadowMaskPass(const Wolf::ResourceNonOwner<ShadowMaskPassInterface>& shadowMaskPassInterface, Wolf::GraphicAPIManager* graphicApiManager);
	void setEnableTrilinearVoxelGI(bool value) { m_enableTrilinearVoxelGI = value; }

private:
	void createOutputImage(const Wolf::InitializationContext& context);
	Wolf::Attachment setupColorAttachment(const Wolf::InitializationContext& context);
	Wolf::Attachment setupDepthAttachment(const Wolf::InitializationContext& context);
	void initializeFramesBuffer(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment);

	void createPipelines();

	std::unique_ptr<Wolf::RenderPass> m_renderPass;
	Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;
	Wolf::ResourceUniqueOwner<Wolf::FrameBuffer> m_frameBuffer;

	uint32_t m_renderWidth;
	uint32_t m_renderHeight;

	/* Shared resources */
	Wolf::DescriptorSetLayoutGenerator m_commonDescriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_commonDescriptorSet;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_commonDescriptorSetLayout;

	/* Display options */
	struct DisplayOptionsUBData
	{
		uint32_t displayType;
		bool enableTrilinearVoxelGI;
	};
	std::unique_ptr<Wolf::UniformBuffer> m_displayOptionsUniformBuffer;

	bool m_enableTrilinearVoxelGI = false;

	/* Particles resources */
	Wolf::DescriptorSetLayoutGenerator m_particlesDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_particlesDescriptorSetLayout;
	std::unique_ptr<Wolf::DescriptorSet> m_particlesDescriptorSet;

	std::unique_ptr<Wolf::ShaderParser> m_particlesVertexShaderParser;
	std::unique_ptr<Wolf::ShaderParser> m_particlesFragmentShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_particlesPipeline;

	static constexpr uint32_t SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES = 5;

	/* Full screen rect */
	std::unique_ptr<Wolf::Mesh> m_fullscreenRect;

	EditorParams* m_editorParams;
	Wolf::ResourceNonOwner<const ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceNonOwner<const ParticleUpdatePass> m_particlesUpdatePass;
	Wolf::NullableResourceNonOwner<ShadowMaskPassInterface> m_shadowMaskPass;
	Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
	Wolf::NullableResourceNonOwner<RayTracedWorldDebugPass> m_rayTracedWorldDebugPass;
	Wolf::NullableResourceNonOwner<PathTracingPass> m_pathTracingPass;
	Wolf::ResourceNonOwner<ComputeSkyCubeMapPass> m_computeSkyCubeMapPass;
	Wolf::ResourceNonOwner<SkyBoxManager> m_skyBoxManager;
	Wolf::ResourceNonOwner<GlobalIrradiancePassInterface> m_globalIrradiancePass;
};

