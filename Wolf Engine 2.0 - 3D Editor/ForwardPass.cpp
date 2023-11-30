#include "ForwardPass.h"

#include <DebugMarker.h>
#include <DescriptorSet.h>
#include <DescriptorSetGenerator.h>
#include <Image.h>

#include "BuildingModel.h"
#include "CommonDescriptorLayouts.h"
#include "GameContext.h"
#include "EditorModelInterface.h"
#include "Vertex2DTextured.h"

using namespace Wolf;

VkDescriptorSetLayout CommonDescriptorLayouts::g_commonDescriptorSetLayout;

void ForwardPass::initializeResources(const InitializationContext& context)
{
	Attachment color = setupColorAttachment(context);
	Attachment depth = setupDepthAttachment(context);

	m_renderPass.reset(new RenderPass({ color, depth }));
	m_commandBuffer.reset(new CommandBuffer(QueueType::GRAPHIC, false /* isTransient */));
	initializeFramesBuffers(context, color, depth);
	
	m_semaphore.reset(new Semaphore(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT));

	// Shared resources
	{
		m_displayOptionsUniformBuffer.reset(new Buffer(sizeof(DisplayOptionsUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, UpdateRate::NEVER));

		m_commonDescriptorSetLayoutGenerator.addUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		m_commonDescriptorSetLayout.reset(new DescriptorSetLayout(m_commonDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		CommonDescriptorLayouts::g_commonDescriptorSetLayout = m_commonDescriptorSetLayout->getDescriptorSetLayout();

		DescriptorSetGenerator descriptorSetGenerator(m_commonDescriptorSetLayoutGenerator.getDescriptorLayouts());
		descriptorSetGenerator.setBuffer(0, *m_displayOptionsUniformBuffer);

		m_commonDescriptorSet.reset(new DescriptorSet(m_commonDescriptorSetLayout->getDescriptorSetLayout(), UpdateRate::NEVER));
		m_commonDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
	}

	// UI resources
	{
		m_userInterfaceDescriptorSetLayoutGenerator.addCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		m_userInterfaceDescriptorSetLayout.reset(new DescriptorSetLayout(m_userInterfaceDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		DescriptorSetGenerator descriptorSetGenerator(m_userInterfaceDescriptorSetLayoutGenerator.getDescriptorLayouts());
		m_userInterfaceSampler.reset(new Sampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 11, VK_FILTER_LINEAR));
		descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, context.userInterfaceImage->getDefaultImageView(), *m_userInterfaceSampler);

		m_userInterfaceDescriptorSet.reset(new DescriptorSet(m_userInterfaceDescriptorSetLayout->getDescriptorSetLayout(), UpdateRate::NEVER));
		m_userInterfaceDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

		// Load fullscreen rect
		const std::vector<Vertex2DTextured> vertices =
		{
			{ glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 0.0f) }, // top left
			{ glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 0.0f) }, // top right
			{ glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 1.0f) }, // bot left
			{ glm::vec2(1.0f, 1.0f),glm::vec2(1.0f, 1.0f) } // bot right
		};

		const std::vector<uint32_t> indices =
		{
			0, 2, 1,
			2, 3, 1
		};

		m_fullscreenRect.reset(new Mesh(vertices, indices));

		m_userInterfaceVertexShaderParser.reset(new ShaderParser("Shaders/UI/UI.vert"));
		m_userInterfaceFragmentShaderParser.reset(new ShaderParser("Shaders/UI/UI.frag"));
	}

	m_renderWidth = context.swapChainWidth;
	m_renderHeight = context.swapChainHeight;
	createPipelines();
}

void ForwardPass::resize(const InitializationContext& context)
{
	m_renderPass->setExtent({ context.swapChainWidth, context.swapChainHeight });

	Attachment color = setupColorAttachment(context);
	Attachment depth = setupDepthAttachment(context);
	initializeFramesBuffers(context, color, depth);

	DescriptorSetGenerator descriptorSetGenerator(m_userInterfaceDescriptorSetLayoutGenerator.getDescriptorLayouts());
	descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, context.userInterfaceImage->getDefaultImageView(), *m_userInterfaceSampler);
	m_userInterfaceDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	m_renderWidth = context.swapChainWidth;
	m_renderHeight = context.swapChainHeight;
	createPipelines(); // may be overkill but resets the viewport for UI
}

void ForwardPass::record(const RecordContext& context)
{
	/* Command buffer record */
	const GameContext* gameContext = static_cast<const GameContext*>(context.gameContext);
	const uint32_t frameBufferIdx = context.swapChainImageIdx;
	const VkCommandBuffer commandBuffer = m_commandBuffer->getCommandBuffer(context.commandBufferIdx);

	m_commandBuffer->beginCommandBuffer(context.commandBufferIdx);

	DebugMarker::beginRegion(commandBuffer, DebugMarker::renderPassDebugColor, "Forward pass");

	std::vector<VkClearValue> clearValues(2);
	clearValues[0] = { 0.1f, 0.1f, 0.1f, 1.0f };
	clearValues[1] = { 1.0f };
	m_renderPass->beginRenderPass(m_frameBuffers[frameBufferIdx]->getFramebuffer(), clearValues, commandBuffer);

	/* Shared resources */
	DisplayOptionsUBData displayOptions{};
	displayOptions.displayType = static_cast<uint32_t>(gameContext->displayType);
	m_displayOptionsUniformBuffer->transferCPUMemory(&displayOptions, sizeof(displayOptions), 0);

	const VkViewport renderViewport = m_editorParams->getRenderViewport();
	vkCmdSetViewport(commandBuffer, 0, 1, &renderViewport);

	context.renderMeshList->draw(context, commandBuffer, m_renderPass.get(), 0, 0,
		{
			{ COMMON_DESCRIPTOR_SET_SLOT, m_commonDescriptorSet.get() }
		});

	/* UI */
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_userInterfacePipeline->getPipeline());
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_userInterfacePipeline->getPipelineLayout(), 0, 1,
		m_userInterfaceDescriptorSet->getDescriptorSet(), 0, nullptr);
	m_fullscreenRect->draw(commandBuffer, RenderMeshList::NO_CAMERA_IDX);

	m_renderPass->endRenderPass(commandBuffer);

	DebugMarker::endRegion(commandBuffer);

	m_commandBuffer->endCommandBuffer(context.commandBufferIdx);
}

void ForwardPass::submit(const SubmitContext& context)
{
	const std::vector waitSemaphores{ context.swapChainImageAvailableSemaphore, context.userInterfaceImageAvailableSemaphore };
	const std::vector signalSemaphores{ m_semaphore->getSemaphore() };
	m_commandBuffer->submit(context.commandBufferIdx, waitSemaphores, signalSemaphores, context.frameFence);

	bool anyShaderModified = m_userInterfaceVertexShaderParser->compileIfFileHasBeenModified();
	if (m_userInterfaceFragmentShaderParser->compileIfFileHasBeenModified())
		anyShaderModified = true;

	if (anyShaderModified)
	{
		vkDeviceWaitIdle(context.device);
		createPipelines();
	}
}

Attachment ForwardPass::setupColorAttachment(const InitializationContext& context)
{
	return Attachment({ context.swapChainWidth, context.swapChainHeight }, context.swapChainFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		nullptr);
}

Attachment ForwardPass::setupDepthAttachment(const InitializationContext& context)
{
	CreateImageInfo depthImageCreateInfo;
	depthImageCreateInfo.format = context.depthFormat;
	depthImageCreateInfo.extent.width = context.swapChainWidth;
	depthImageCreateInfo.extent.height = context.swapChainHeight;
	depthImageCreateInfo.extent.depth = 1;
	depthImageCreateInfo.mipLevelCount = 1;
	depthImageCreateInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	m_depthImage.reset(new Image(depthImageCreateInfo));

	return Attachment({ context.swapChainWidth, context.swapChainHeight }, context.depthFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_STORE,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, m_depthImage->getDefaultImageView());
}

void ForwardPass::initializeFramesBuffers(const InitializationContext& context, Attachment& colorAttachment, Attachment& depthAttachment)
{
	m_frameBuffers.clear();
	m_frameBuffers.resize(context.swapChainImageCount);
	for (uint32_t i = 0; i < context.swapChainImageCount; ++i)
	{
		colorAttachment.imageView = context.swapChainImages[i]->getDefaultImageView();
		m_frameBuffers[i].reset(new Framebuffer(m_renderPass->getRenderPass(), { colorAttachment,  depthAttachment }));
	}
}

void ForwardPass::createPipelines()
{
	// UI
	{
		RenderingPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.renderPass = m_renderPass->getRenderPass();

		// Programming stages
		pipelineCreateInfo.shaderCreateInfos.resize(2);
		m_userInterfaceVertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
		pipelineCreateInfo.shaderCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		m_userInterfaceFragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
		pipelineCreateInfo.shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		// IA
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
		Vertex2DTextured::getAttributeDescriptions(attributeDescriptions, 0);
		pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0] = {};
		Vertex2DTextured::getBindingDescription(bindingDescriptions[0], 0);
		pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

		// Viewport
		pipelineCreateInfo.extent = { m_renderWidth, m_renderHeight };

		// Resources
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts = { m_userInterfaceDescriptorSetLayout->getDescriptorSetLayout() };
		pipelineCreateInfo.descriptorSetLayouts = descriptorSetLayouts;

		// Color Blend
		std::vector<RenderingPipelineCreateInfo::BLEND_MODE> blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ALPHA };
		pipelineCreateInfo.blendModes = blendModes;

		m_userInterfacePipeline.reset(new Pipeline(pipelineCreateInfo));
	}
}
