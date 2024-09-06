#pragma once

#include "Attachment.h"
#include "CommandRecordBase.h"
#include "ResourceUniqueOwner.h"

#include "PreDepthPass.h"

class ParticleSeparateRenderPass : public Wolf::CommandRecordBase
{
public:
	ParticleSeparateRenderPass(const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass);

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

private:
	Wolf::Attachment setupColorAttachment(const Wolf::InitializationContext& context);
	Wolf::Attachment setupDepthAttachment(const Wolf::InitializationContext& context);
	void initializeFramesBuffer(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment);

	Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;

	Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;
	std::unique_ptr<Wolf::RenderPass> m_renderPass;
	Wolf::ResourceUniqueOwner<Wolf::FrameBuffer> m_frameBuffer;
};

