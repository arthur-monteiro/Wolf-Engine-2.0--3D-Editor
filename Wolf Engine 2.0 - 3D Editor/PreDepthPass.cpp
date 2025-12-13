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
	createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

	DepthPassBase::initializeResources(context);
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

	DepthPassBase::record(context);

	m_commandBuffer->endCommandBuffer();
}

void PreDepthPass::submit(const Wolf::SubmitContext& context)
{
	std::vector<const Wolf::Semaphore*> waitSemaphores{ };
	if (m_updateGPUBuffersPass->transferRecordedThisFrame())
		waitSemaphores.push_back(m_updateGPUBuffersPass->getSemaphore(context.swapChainImageIndex));
	if (m_computeVertexDataPass->hasCommandsRecordedThisFrame())
		waitSemaphores.push_back(m_computeVertexDataPass->getSemaphore(context.swapChainImageIndex));

	const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };

	m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);
}

void PreDepthPass::recordDraws(const Wolf::RecordContext& context)
{
	const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();
	m_commandBuffer->setViewport(renderViewport);

	context.renderMeshList->draw(context, *m_commandBuffer, m_renderPass.get(), CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH, CommonCameraIndices::CAMERA_IDX_MAIN,
		{}, {});
}

const Wolf::CommandBuffer& PreDepthPass::getCommandBuffer(const Wolf::RecordContext& context)
{
	return *m_commandBuffer;
}
