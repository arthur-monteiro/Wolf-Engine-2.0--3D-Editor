#include "PreDepthPass.h"

#include <DebugMarker.h>
#include <DefaultMeshRenderer.h>
#include <InstanceMeshRenderer.h>
#include <ProfilerCommon.h>
#include <Timer.h>

#include "CommonLayouts.h"

void PreDepthPass::initializeResources(const Wolf::InitializationContext& context)
{
	Wolf::Timer timer("Depth pass initialization");

	m_swapChainWidth = context.swapChainWidth;
	m_swapChainHeight = context.swapChainHeight;

	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false, "Pre depth"));
	createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

	DepthPassBase::initializeResources(context);
	m_depthImage->setName("Pre depth pass output (PreDepthPass::m_depthImage)");

	m_depthImage->setImageLayout({ Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, 0, 1, Wolf::ImageLayout::UNDEFINED });
}

void PreDepthPass::resize(const Wolf::InitializationContext& context)
{
	m_swapChainWidth = context.swapChainWidth;
	m_swapChainHeight = context.swapChainHeight;

	DepthPassBase::resize(context);
}

void PreDepthPass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "PreDepth pass");

	m_depthImage->transitionImageLayout(*m_commandBuffer, { Wolf::ImageLayout::TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 1, 0, 1, Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL });

	VkImageSubresourceRange range{};
	range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	m_commandBuffer->clearDepthStencilImage(*m_depthImage, Wolf::ImageLayout::TRANSFER_DST_OPTIMAL, 1.0f, range);

	DepthPassBase::record(context);

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void PreDepthPass::submit(const Wolf::SubmitContext& context)
{
	std::vector<const Wolf::Semaphore*> waitSemaphores{ context.instanceRendererBuffersAvailableSemaphore };
	if (m_updateGPUBuffersPass->transferRecordedThisFrame())
		waitSemaphores.push_back(m_updateGPUBuffersPass->getSemaphore(context.swapChainImageIndex));
	if (m_computeVertexDataPass && m_computeVertexDataPass->hasCommandsRecordedThisFrame())
		waitSemaphores.push_back(m_computeVertexDataPass->getSemaphore(context.swapChainImageIndex));
	if (m_customSceneRenderPass->commandsRecordedThisFrame())
	{
		waitSemaphores.push_back(m_customSceneRenderPass->getSemaphore(context.swapChainImageIndex));
	}

	const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };

	m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);
}

void PreDepthPass::recordDraws(const Wolf::RecordContext& context)
{
	const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();
	m_commandBuffer->setViewport(renderViewport);

	context.m_defaultMeshRenderer->draw(context, *m_commandBuffer, m_renderPass.get(), CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH, CommonCameraIndices::CAMERA_IDX_MAIN,
		{}, {});

	context.m_instanceMeshRenderer->draw(context, *m_commandBuffer, m_renderPass.get(), CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH, CommonCameraIndices::CAMERA_IDX_MAIN,
		{}, {});
}

const Wolf::CommandBuffer& PreDepthPass::getCommandBuffer(const Wolf::RecordContext& context)
{
	return *m_commandBuffer;
}

Wolf::Viewport PreDepthPass::getViewport()
{
	return m_editorParams->getRenderViewport();
}
