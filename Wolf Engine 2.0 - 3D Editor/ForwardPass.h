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

class ForwardPass : public Wolf::CommandRecordBase
{
public:
	ForwardPass(BindlessDescriptor* bindlessDescriptor, EditorParams* editorParams) : m_bindlessDescriptor(bindlessDescriptor), m_editorParams(editorParams) {}

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

private:
	static Wolf::Attachment setupColorAttachment(const Wolf::InitializationContext& context);
	Wolf::Attachment setupDepthAttachment(const Wolf::InitializationContext& context);
	void initializeFramesBuffers(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment);

	void createDefaultDescriptorSetLayout();
	void createBuildingDescriptorSetLayout();
	void createPipelines();

	std::unique_ptr<Wolf::RenderPass> m_renderPass;
	std::vector<std::unique_ptr<Wolf::Framebuffer>> m_frameBuffers;
	std::unique_ptr<Wolf::Image> m_depthImage;

	uint32_t m_renderWidth;
	uint32_t m_renderHeight;

	/* Default pipeline */
	std::unique_ptr<Wolf::ShaderParser> m_defaultVertexShaderParser;
	std::unique_ptr<Wolf::ShaderParser> m_defaultFragmentShaderParser;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_defaultDescriptorSetLayout;
	std::unique_ptr<Wolf::Pipeline> m_defaultPipeline;

	/* Building pipeline */
	std::unique_ptr<Wolf::ShaderParser> m_buildingVertexShaderParser;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_buildingDescriptorSetLayout;
	std::unique_ptr<Wolf::Pipeline> m_buildingPipeline;

	/* UI resources */
	Wolf::DescriptorSetLayoutGenerator m_userInterfaceDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::DescriptorSetLayout> m_userInterfaceDescriptorSetLayout;
	std::unique_ptr<Wolf::DescriptorSet> m_userInterfaceDescriptorSet;
	std::unique_ptr<Wolf::Mesh> m_fullscreenRect;
	std::unique_ptr<Wolf::Sampler> m_userInterfaceSampler;

	std::unique_ptr<Wolf::ShaderParser> m_userInterfaceVertexShaderParser;
	std::unique_ptr<Wolf::ShaderParser> m_userInterfaceFragmentShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_userInterfacePipeline;

	BindlessDescriptor* m_bindlessDescriptor;
	EditorParams* m_editorParams;
};

