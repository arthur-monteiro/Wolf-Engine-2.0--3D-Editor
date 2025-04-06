#include "ParticleSeparateRenderPass.h"

#include "Attachment.h"
#include "CameraList.h"
#include "DebugMarker.h"
#include "ForwardPass.h"
#include "FrameBuffer.h"
#include "Image.h"
#include "RenderPass.h"

ParticleSeparateRenderPass::ParticleSeparateRenderPass(const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass) : m_preDepthPass(preDepthPass)
{}

void ParticleSeparateRenderPass::initializeResources(const Wolf::InitializationContext& context)
{
	Wolf::Attachment color = setupColorAttachment(context);
	Wolf::Attachment depth = setupDepthAttachment(context);

	m_renderPass.reset(Wolf::RenderPass::createRenderPass({ color, depth }));
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false));
	initializeFramesBuffer(context, color, depth);

	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT));
}

void ParticleSeparateRenderPass::resize(const Wolf::InitializationContext& context)
{
	m_renderPass->setExtent({ context.swapChainWidth, context.swapChainHeight });

	Wolf::Attachment color = setupColorAttachment(context);
	Wolf::Attachment depth = setupDepthAttachment(context);
	initializeFramesBuffer(context, color, depth);
}

void ParticleSeparateRenderPass::record(const Wolf::RecordContext& context)
{
	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Particle render separated pass");

	std::vector<Wolf::ClearValue> clearValues(2);
	clearValues[0] = { 0.1f, 0.1f, 0.1f, 1.0f };
	clearValues[1] = { 1.0f };
	m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffer, clearValues);

	// Render particles
	
	m_commandBuffer->endRenderPass();

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void ParticleSeparateRenderPass::submit(const Wolf::SubmitContext& context)
{

}

Wolf::Attachment ParticleSeparateRenderPass::setupColorAttachment(const Wolf::InitializationContext& context)
{
	Wolf::CreateImageInfo depthImageCreateInfo;
	depthImageCreateInfo.format = Wolf::Format::R8G8B8A8_UNORM;
	depthImageCreateInfo.extent.width = context.swapChainWidth;
	depthImageCreateInfo.extent.height = context.swapChainHeight;
	depthImageCreateInfo.extent.depth = 1;
	depthImageCreateInfo.mipLevelCount = 1;
	depthImageCreateInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	depthImageCreateInfo.usage = Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT;
	m_outputImage.reset(Wolf::Image::createImage(depthImageCreateInfo));

	return Wolf::Attachment({ context.swapChainWidth, context.swapChainHeight }, Wolf::Format::R8G8B8A8_UNORM, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, Wolf::AttachmentStoreOp::STORE,
		Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT, m_outputImage->getDefaultImageView());
}

Wolf::Attachment ParticleSeparateRenderPass::setupDepthAttachment(const Wolf::InitializationContext& context)
{
	Wolf::Attachment depth({ context.swapChainWidth, context.swapChainHeight }, context.depthFormat, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, Wolf::AttachmentStoreOp::DONT_CARE,
		Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT, m_preDepthPass->getOutput()->getDefaultImageView());
	depth.loadOperation = Wolf::AttachmentLoadOp::LOAD;
	depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	return depth;
}

void ParticleSeparateRenderPass::initializeFramesBuffer(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment)
{
	colorAttachment.imageView = m_outputImage->getDefaultImageView();
	m_frameBuffer.reset(Wolf::FrameBuffer::createFrameBuffer(*m_renderPass, { colorAttachment,  depthAttachment }));
}
