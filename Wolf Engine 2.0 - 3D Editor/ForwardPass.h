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

#include "BindlessDescriptor.h"
#include "EditorParams.h"
#include "ParticleUpdatePass.h"

class LightManager;

class ForwardPass : public Wolf::CommandRecordBase
{
public:
	ForwardPass(EditorParams* editorParams, const Wolf::Semaphore* contaminationUpdateSemaphore, const Wolf::ResourceNonOwner<const ParticleUpdatePass>& particlesUpdateSemaphore) :
		m_editorParams(editorParams), m_contaminationUpdateSemaphore(contaminationUpdateSemaphore), m_particlesUpdatePass(particlesUpdateSemaphore) {}

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void updateLights(const Wolf::ResourceNonOwner<LightManager>& lightManager) const;

private:
	static Wolf::Attachment setupColorAttachment(const Wolf::InitializationContext& context);
	Wolf::Attachment setupDepthAttachment(const Wolf::InitializationContext& context);
	void initializeFramesBuffers(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment);

	void createPipelines();

	std::unique_ptr<Wolf::RenderPass> m_renderPass;
	std::vector<std::unique_ptr<Wolf::FrameBuffer>> m_frameBuffers;
	std::unique_ptr<Wolf::Image> m_depthImage;

	uint32_t m_renderWidth;
	uint32_t m_renderHeight;

	/* Shared resources */
	Wolf::DescriptorSetLayoutGenerator m_commonDescriptorSetLayoutGenerator;
	static constexpr uint32_t COMMON_DESCRIPTOR_SET_SLOT = 2;
	std::unique_ptr<Wolf::DescriptorSet> m_commonDescriptorSet;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_commonDescriptorSetLayout;

	// Display options
	struct DisplayOptionsUBData
	{
		uint32_t displayType;
	};
	std::unique_ptr<Wolf::Buffer> m_displayOptionsUniformBuffer;

	// Lights
	struct PointLightInfo
	{
		glm::vec4 lightPos;
		glm::vec4 lightColor;
	};
	static constexpr uint32_t MAX_POINT_LIGHTS = 16;

	struct SunLightInfo
	{
		glm::vec4 sunDirection;
		glm::vec4 sunColor;
	};
	static constexpr uint32_t MAX_SUN_LIGHTS = 1;

	struct LightsUBData
	{
		PointLightInfo pointLights[MAX_POINT_LIGHTS];
		uint32_t pointLightsCount;
		glm::vec3 padding;

		SunLightInfo sunLights[MAX_SUN_LIGHTS];
		uint32_t sunLightsCount;
		glm::vec3 padding2;
	};
	std::unique_ptr<Wolf::Buffer> m_pointLightsUniformBuffer;

	/* Particles resources */
	Wolf::DescriptorSetLayoutGenerator m_particlesDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_particlesDescriptorSetLayout;
	std::unique_ptr<Wolf::DescriptorSet> m_particlesDescriptorSet;

	std::unique_ptr<Wolf::ShaderParser> m_particlesVertexShaderParser;
	std::unique_ptr<Wolf::ShaderParser> m_particlesFragmentShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_particlesPipeline;

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
	const Wolf::Semaphore* m_contaminationUpdateSemaphore;
	Wolf::ResourceNonOwner<const ParticleUpdatePass> m_particlesUpdatePass;
};

