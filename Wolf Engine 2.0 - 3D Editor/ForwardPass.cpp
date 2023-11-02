#include "ForwardPass.h"

#include <DebugMarker.h>
#include <DescriptorSetGenerator.h>
#include <Image.h>

#include "BuildingModel.h"
#include "DefaultPipelineInfos.h"
#include "GameContext.h"
#include "ModelInterface.h"
#include "ObjLoader.h"
#include "Vertex2DTextured.h"

Wolf::DescriptorSetLayoutGenerator DefaultPipeline::g_descriptorSetLayoutGenerator;
VkDescriptorSetLayout DefaultPipeline::g_descriptorSetLayout = nullptr;

using namespace Wolf;

void ForwardPass::initializeResources(const Wolf::InitializationContext& context)
{
	Attachment color = setupColorAttachment(context);
	Attachment depth = setupDepthAttachment(context);

	m_renderPass.reset(new RenderPass({ color, depth }));
	m_commandBuffer.reset(new CommandBuffer(QueueType::GRAPHIC, false /* isTransient */));
	initializeFramesBuffers(context, color, depth);
	
	m_semaphore.reset(new Semaphore(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT));

	// Default pipeline
	{
		m_defaultVertexShaderParser.reset(new ShaderParser("Shaders/defaultPipeline/shader.vert"));
		m_defaultFragmentShaderParser.reset(new ShaderParser("Shaders/defaultPipeline/shader.frag"));

		createDefaultDescriptorSetLayout();
	}

	// Building pipeline
	{
		m_buildingVertexShaderParser.reset(new ShaderParser("Shaders/buildings/shader.vert"));

		createBuildingDescriptorSetLayout();
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

void ForwardPass::resize(const Wolf::InitializationContext& context)
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

void ForwardPass::record(const Wolf::RecordContext& context)
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

	/* Default pipeline */
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_defaultPipeline->getPipeline());
	m_bindlessDescriptor->bind(commandBuffer, m_defaultPipeline->getPipelineLayout());

	const VkViewport renderViewport = m_editorParams->getRenderViewport();
	vkCmdSetViewport(commandBuffer, 0, 1, &renderViewport);

	for(const ModelInterface* model : gameContext->m_modelsToRenderWithDefaultPipeline)
	{
		model->draw(commandBuffer, m_defaultPipeline->getPipelineLayout());
	}

	/* Building pipeline */
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_buildingPipeline->getPipeline());
	m_bindlessDescriptor->bind(commandBuffer, m_buildingPipeline->getPipelineLayout());

	vkCmdSetViewport(commandBuffer, 0, 1, &renderViewport);

	for (const ModelInterface* model : gameContext->m_modelsToRenderWithBuildingPipeline)
	{
		model->draw(commandBuffer, m_buildingPipeline->getPipelineLayout());
	}

	/* UI */
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_userInterfacePipeline->getPipeline());
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_userInterfacePipeline->getPipelineLayout(), 0, 1,
		m_userInterfaceDescriptorSet->getDescriptorSet(), 0, nullptr);
	m_fullscreenRect->draw(commandBuffer);

	m_renderPass->endRenderPass(commandBuffer);

	DebugMarker::endRegion(commandBuffer);

	m_commandBuffer->endCommandBuffer(context.commandBufferIdx);
}

void ForwardPass::submit(const Wolf::SubmitContext& context)
{
	const std::vector waitSemaphores{ context.swapChainImageAvailableSemaphore, context.userInterfaceImageAvailableSemaphore };
	const std::vector signalSemaphores{ m_semaphore->getSemaphore() };
	m_commandBuffer->submit(context.commandBufferIdx, waitSemaphores, signalSemaphores, context.frameFence);

	bool anyShaderModified = m_defaultFragmentShaderParser->compileIfFileHasBeenModified();
	if (m_defaultFragmentShaderParser->compileIfFileHasBeenModified())
		anyShaderModified = true;
	if (m_buildingVertexShaderParser->compileIfFileHasBeenModified())
		anyShaderModified = true;

	if (anyShaderModified)
	{
		vkDeviceWaitIdle(context.device);
		createPipelines();
	}
}

Wolf::Attachment ForwardPass::setupColorAttachment(const Wolf::InitializationContext& context)
{
	return Attachment({ context.swapChainWidth, context.swapChainHeight }, context.swapChainFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		nullptr);
}

Wolf::Attachment ForwardPass::setupDepthAttachment(const Wolf::InitializationContext& context)
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

void ForwardPass::initializeFramesBuffers(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment)
{
	m_frameBuffers.clear();
	m_frameBuffers.resize(context.swapChainImageCount);
	for (uint32_t i = 0; i < context.swapChainImageCount; ++i)
	{
		colorAttachment.imageView = context.swapChainImages[i]->getDefaultImageView();
		m_frameBuffers[i].reset(new Framebuffer(m_renderPass->getRenderPass(), { colorAttachment,  depthAttachment }));
	}
}

void ForwardPass::createDefaultDescriptorSetLayout()
{
	DefaultPipeline::g_descriptorSetLayoutGenerator.reset();
	DefaultPipeline::g_descriptorSetLayoutGenerator.addUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0); // matrices
	
	m_defaultDescriptorSetLayout.reset(new DescriptorSetLayout(DefaultPipeline::g_descriptorSetLayoutGenerator.getDescriptorLayouts()));
	DefaultPipeline::g_descriptorSetLayout = m_defaultDescriptorSetLayout->getDescriptorSetLayout();
}

void ForwardPass::createBuildingDescriptorSetLayout()
{
	Building::g_descriptorSetLayoutGenerator.reset();
	Building::g_descriptorSetLayoutGenerator.addUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0); // mesh infos

	m_buildingDescriptorSetLayout.reset(new DescriptorSetLayout(Building::g_descriptorSetLayoutGenerator.getDescriptorLayouts()));
	Building::g_descriptorSetLayout = m_buildingDescriptorSetLayout->getDescriptorSetLayout();
}

void ForwardPass::createPipelines()
{
	// Default pipeline
	{
		RenderingPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.renderPass = m_renderPass->getRenderPass();

		// Programming stages
		std::vector<char> vertexShaderCode;
		m_defaultVertexShaderParser->readCompiledShader(vertexShaderCode);
		std::vector<char> fragmentShaderCode;
		m_defaultFragmentShaderParser->readCompiledShader(fragmentShaderCode);

		std::vector<ShaderCreateInfo> shaders(2);
		shaders[0].shaderCode = vertexShaderCode;
		shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaders[1].shaderCode = fragmentShaderCode;
		shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		pipelineCreateInfo.shaderCreateInfos = shaders;

		// IA
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
		Vertex3D::getAttributeDescriptions(attributeDescriptions, 0);
		pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0] = {};
		Vertex3D::getBindingDescription(bindingDescriptions[0], 0);
		pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

		// Resources
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts = { m_bindlessDescriptor->getDescriptorSetLayout(), m_defaultDescriptorSetLayout->getDescriptorSetLayout() };
		pipelineCreateInfo.descriptorSetLayouts = descriptorSetLayouts;

		// Viewport
		pipelineCreateInfo.extent = { m_renderWidth, m_renderHeight };

		// Color Blend
		std::vector<RenderingPipelineCreateInfo::BLEND_MODE> blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };
		pipelineCreateInfo.blendModes = blendModes;

		// Dynamic states
		pipelineCreateInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		m_defaultPipeline.reset(new Pipeline(pipelineCreateInfo));
	}

	// Building pipeline
	{
		RenderingPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.renderPass = m_renderPass->getRenderPass();

		// Programming stages
		std::vector<char> vertexShaderCode;
		m_buildingVertexShaderParser->readCompiledShader(vertexShaderCode);
		std::vector<char> fragmentShaderCode;
		m_defaultFragmentShaderParser->readCompiledShader(fragmentShaderCode);

		std::vector<ShaderCreateInfo> shaders(2);
		shaders[0].shaderCode = vertexShaderCode;
		shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaders[1].shaderCode = fragmentShaderCode;
		shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		pipelineCreateInfo.shaderCreateInfos = shaders;

		// IA
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
		Vertex3D::getAttributeDescriptions(attributeDescriptions, 0);
		BuildingModel::InstanceData::getAttributeDescriptions(attributeDescriptions, 1, attributeDescriptions.size());
		pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

		std::vector<VkVertexInputBindingDescription> bindingDescriptions(2);
		bindingDescriptions[0] = {};
		Vertex3D::getBindingDescription(bindingDescriptions[0], 0);
		BuildingModel::InstanceData::getBindingDescription(bindingDescriptions[1], 1);
		pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

		// Resources
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts = { m_bindlessDescriptor->getDescriptorSetLayout(), DefaultPipeline::g_descriptorSetLayout, Building::g_descriptorSetLayout };
		pipelineCreateInfo.descriptorSetLayouts = descriptorSetLayouts;

		// Viewport
		pipelineCreateInfo.extent = { m_renderWidth, m_renderHeight };

		// Color Blend
		std::vector<RenderingPipelineCreateInfo::BLEND_MODE> blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };
		pipelineCreateInfo.blendModes = blendModes;

		// Dynamic states
		pipelineCreateInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		pipelineCreateInfo.cullMode = VK_CULL_MODE_NONE; // temp

		m_buildingPipeline.reset(new Pipeline(pipelineCreateInfo));
	}

	// UI
	{
		RenderingPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.renderPass = m_renderPass->getRenderPass();

		// Programming stages
		std::vector<char> vertexShaderCode;
		m_userInterfaceVertexShaderParser->readCompiledShader(vertexShaderCode);
		std::vector<char> fragmentShaderCode;
		m_userInterfaceFragmentShaderParser->readCompiledShader(fragmentShaderCode);

		std::vector<ShaderCreateInfo> shaders(2);
		shaders[0].shaderCode = vertexShaderCode;
		shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaders[1].shaderCode = fragmentShaderCode;
		shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		pipelineCreateInfo.shaderCreateInfos = shaders;

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
