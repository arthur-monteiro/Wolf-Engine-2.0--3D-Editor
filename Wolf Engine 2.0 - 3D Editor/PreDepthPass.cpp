#include "PreDepthPass.h"

#include <ProfilerCommon.h>
#include <RenderMeshList.h>
#include <Timer.h>

#include "CommonLayouts.h"

void PreDepthPass::initializeResources(const Wolf::InitializationContext& context)
{
	Wolf::Timer timer("Depth pass initialization");

	m_swapChainWidth = context.swapChainWidth;
	m_swapChainHeight = context.swapChainHeight;

	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false));
	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

	DepthPassBase::initializeResources(context);

	createCopyImage(context.depthFormat);
}

void PreDepthPass::resize(const Wolf::InitializationContext& context)
{
	m_swapChainWidth = context.swapChainWidth;
	m_swapChainHeight = context.swapChainHeight;

	DepthPassBase::resize(context);

	createCopyImage(context.depthFormat);
}

void PreDepthPass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	m_commandBuffer->beginCommandBuffer();

	DepthPassBase::record(context);

	VkImageCopy copyRegion{};

	copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	copyRegion.srcSubresource.baseArrayLayer = 0;
	copyRegion.srcSubresource.mipLevel = 0;
	copyRegion.srcSubresource.layerCount = 1;
	copyRegion.srcOffset = { 0, 0, 0 };

	copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	copyRegion.dstSubresource.baseArrayLayer = 0;
	copyRegion.dstSubresource.mipLevel = 0;
	copyRegion.dstSubresource.layerCount = 1;
	copyRegion.dstOffset = { 0, 0, 0 };

	copyRegion.extent = m_copyImage->getExtent();

	m_depthImage->setImageLayoutWithoutOperation(getFinalLayout()); // at this point, render pass should have set final layout

	m_copyImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1 });
	m_copyImage->recordCopyGPUImage(*m_depthImage, copyRegion, *m_commandBuffer);
	m_copyImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1 });
	m_depthImage->transitionImageLayout(*m_commandBuffer, { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });

	m_commandBuffer->endCommandBuffer();
}

void PreDepthPass::submit(const Wolf::SubmitContext& context)
{
	std::vector<const Wolf::Semaphore*> waitSemaphores{ };
	if (m_updateGPUBuffersPass->transferRecordedThisFrame())
		waitSemaphores.push_back(m_updateGPUBuffersPass->getSemaphore());

	const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };

	m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);
}

void PreDepthPass::createCopyImage(VkFormat format)
{
	Wolf::CreateImageInfo depthCopyImageCreateInfo;
	depthCopyImageCreateInfo.format = format;
	depthCopyImageCreateInfo.extent.width = getWidth();
	depthCopyImageCreateInfo.extent.height = getHeight();
	depthCopyImageCreateInfo.extent.depth = 1;
	depthCopyImageCreateInfo.mipLevelCount = 1;
	depthCopyImageCreateInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthCopyImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	m_copyImage.reset(Wolf::Image::createImage(depthCopyImageCreateInfo));
}

void PreDepthPass::recordDraws(const Wolf::RecordContext& context)
{
	const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();
	m_commandBuffer->setViewport(renderViewport);

	context.renderMeshList->draw(context, *m_commandBuffer, m_renderPass.get(), CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH, CommonCameraIndices::CAMERA_IDX_MAIN,
		{});
}

const Wolf::CommandBuffer& PreDepthPass::getCommandBuffer(const Wolf::RecordContext& context)
{
	return *m_commandBuffer;
}
