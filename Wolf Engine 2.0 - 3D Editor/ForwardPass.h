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
#include "ParticleUpdatePass.h"
#include "PreDepthPass.h"
#include "RayTracedWorldDebugPass.h"
#include "ShadowMaskPassInterface.h"

class ForwardPass : public Wolf::CommandRecordBase
{
public:
	ForwardPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<const ContaminationUpdatePass>& contaminationUpdatePass, const Wolf::ResourceNonOwner<const ParticleUpdatePass>& particlesUpdatePass,
		const Wolf::ResourceNonOwner<ShadowMaskPassInterface>& shadowMaskPass, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<RayTracedWorldDebugPass>& rayTracedWorldDebugPass)
	: m_editorParams(editorParams), m_contaminationUpdatePass(contaminationUpdatePass), m_particlesUpdatePass(particlesUpdatePass), m_shadowMaskPass(shadowMaskPass), m_preDepthPass(preDepthPass),
	  m_rayTracedWorldDebugPass(rayTracedWorldDebugPass)
	{}

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

private:
	static Wolf::Attachment setupColorAttachment(const Wolf::InitializationContext& context);
	Wolf::Attachment setupDepthAttachment(const Wolf::InitializationContext& context);
	void initializeFramesBuffers(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment);

	void createPipelines();

	std::unique_ptr<Wolf::RenderPass> m_renderPass;
	std::vector<std::unique_ptr<Wolf::FrameBuffer>> m_frameBuffers;

	uint32_t m_renderWidth;
	uint32_t m_renderHeight;

	/* Shared resources */
	Wolf::DescriptorSetLayoutGenerator m_commonDescriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_commonDescriptorSet;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_commonDescriptorSetLayout;

	// Display options
	struct DisplayOptionsUBData
	{
		uint32_t displayType;
	};
	std::unique_ptr<Wolf::UniformBuffer> m_displayOptionsUniformBuffer;

	/* Particles resources */
	Wolf::DescriptorSetLayoutGenerator m_particlesDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_particlesDescriptorSetLayout;
	std::unique_ptr<Wolf::DescriptorSet> m_particlesDescriptorSet;

	std::unique_ptr<Wolf::ShaderParser> m_particlesVertexShaderParser;
	std::unique_ptr<Wolf::ShaderParser> m_particlesFragmentShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_particlesPipeline;

	static constexpr uint32_t SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES = 4;

	/* UI resources */
	Wolf::DescriptorSetLayoutGenerator m_userInterfaceDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_userInterfaceDescriptorSetLayout;
	std::unique_ptr<Wolf::DescriptorSet> m_userInterfaceDescriptorSet;
	std::unique_ptr<Wolf::Mesh> m_fullscreenRect;
	std::unique_ptr<Wolf::Sampler> m_userInterfaceSampler;

	std::unique_ptr<Wolf::ShaderParser> m_userInterfaceVertexShaderParser;
	std::unique_ptr<Wolf::ShaderParser> m_userInterfaceFragmentShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_userInterfacePipeline;

	EditorParams* m_editorParams;
	Wolf::ResourceNonOwner<const ContaminationUpdatePass> m_contaminationUpdatePass;
	Wolf::ResourceNonOwner<const ParticleUpdatePass> m_particlesUpdatePass;
	Wolf::ResourceNonOwner<ShadowMaskPassInterface> m_shadowMaskPass;
	Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
	Wolf::ResourceNonOwner<RayTracedWorldDebugPass> m_rayTracedWorldDebugPass;
};

