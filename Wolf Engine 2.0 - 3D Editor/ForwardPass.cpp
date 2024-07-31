#include "ForwardPass.h"

#include <DebugMarker.h>
#include <DescriptorSet.h>
#include <DescriptorSetGenerator.h>
#include <GraphicCameraInterface.h>
#include <Image.h>

#include "BuildingModel.h"
#include "CommonDescriptorLayouts.h"
#include "GameContext.h"
#include "EditorModelInterface.h"

#include "LightManager.h"
#include "Vertex2DTextured.h"

using namespace Wolf;

const DescriptorSetLayout* CommonDescriptorLayouts::g_commonDescriptorSetLayout;

void ForwardPass::initializeResources(const InitializationContext& context)
{
	Attachment color = setupColorAttachment(context);
	Attachment depth = setupDepthAttachment(context);

	m_renderPass.reset(RenderPass::createRenderPass({ color, depth }));
	m_commandBuffer.reset(CommandBuffer::createCommandBuffer(QueueType::GRAPHIC, false /* isTransient */));
	initializeFramesBuffers(context, color, depth);
	
	m_semaphore.reset(Semaphore::createSemaphore(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT));

	// Shared resources
	{
		m_displayOptionsUniformBuffer.reset(Buffer::createBuffer(sizeof(DisplayOptionsUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		m_pointLightsUniformBuffer.reset(Buffer::createBuffer(sizeof(PointLightsUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

		m_commonDescriptorSetLayoutGenerator.addUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		m_commonDescriptorSetLayoutGenerator.addUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
		m_commonDescriptorSetLayout.reset(DescriptorSetLayout::createDescriptorSetLayout(m_commonDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		CommonDescriptorLayouts::g_commonDescriptorSetLayout = m_commonDescriptorSetLayout.get();

		DescriptorSetGenerator descriptorSetGenerator(m_commonDescriptorSetLayoutGenerator.getDescriptorLayouts());
		descriptorSetGenerator.setBuffer(0, *m_displayOptionsUniformBuffer);
		descriptorSetGenerator.setBuffer(1, *m_pointLightsUniformBuffer);

		m_commonDescriptorSet.reset(DescriptorSet::createDescriptorSet(*m_commonDescriptorSetLayout));
		m_commonDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
	}

	// Particles resources
	{
		m_particlesDescriptorSetLayoutGenerator.addStorageBuffer(VK_SHADER_STAGE_VERTEX_BIT,   0); // particles buffer
		m_particlesDescriptorSetLayoutGenerator.addStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1); // emitters buffer
		m_particlesDescriptorSetLayout.reset(DescriptorSetLayout::createDescriptorSetLayout(m_particlesDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		DescriptorSetGenerator descriptorSetGenerator(m_particlesDescriptorSetLayoutGenerator.getDescriptorLayouts());
		descriptorSetGenerator.setBuffer(0, m_particlesUpdatePass->getParticleBuffer());
		descriptorSetGenerator.setBuffer(1, m_particlesUpdatePass->getEmittersBuffer());

		m_particlesDescriptorSet.reset(DescriptorSet::createDescriptorSet(*m_particlesDescriptorSetLayout));
		m_particlesDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

		m_particlesVertexShaderParser.reset(new ShaderParser("Shaders/particles/render.vert", {}, 1));
		m_particlesFragmentShaderParser.reset(new ShaderParser("Shaders/particles/render.frag", {}, -1, 2));
	}

	// UI resources
	{
		m_userInterfaceDescriptorSetLayoutGenerator.addCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		m_userInterfaceDescriptorSetLayout.reset(DescriptorSetLayout::createDescriptorSetLayout(m_userInterfaceDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		DescriptorSetGenerator descriptorSetGenerator(m_userInterfaceDescriptorSetLayoutGenerator.getDescriptorLayouts());
		m_userInterfaceSampler.reset(Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 11, VK_FILTER_LINEAR));
		descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, context.userInterfaceImage->getDefaultImageView(), *m_userInterfaceSampler);

		m_userInterfaceDescriptorSet.reset(DescriptorSet::createDescriptorSet(*m_userInterfaceDescriptorSetLayout));
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

	m_commandBuffer->beginCommandBuffer();

	DebugMarker::beginRegion(m_commandBuffer.get(), DebugMarker::renderPassDebugColor, "Forward pass");

	std::vector<VkClearValue> clearValues(2);
	clearValues[0] = { 0.1f, 0.1f, 0.1f, 1.0f };
	clearValues[1] = { 1.0f };
	m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffers[frameBufferIdx], clearValues);

	/* Shared resources */
	DisplayOptionsUBData displayOptions{};
	displayOptions.displayType = static_cast<uint32_t>(gameContext->displayType);
	m_displayOptionsUniformBuffer->transferCPUMemory(&displayOptions, sizeof(displayOptions), 0);

	const Viewport renderViewport = m_editorParams->getRenderViewport();
	m_commandBuffer->setViewport(renderViewport);

	context.renderMeshList->draw(context, *m_commandBuffer, m_renderPass.get(), 0, 0,
		{
			{ COMMON_DESCRIPTOR_SET_SLOT, m_commonDescriptorSet.get() }
		});

	/* Particles */
	m_commandBuffer->bindPipeline(m_particlesPipeline.get());
	m_commandBuffer->setViewport(renderViewport);
	m_commandBuffer->bindDescriptorSet(m_particlesDescriptorSet.get(), 0, *m_particlesPipeline);
	m_commandBuffer->bindDescriptorSet(context.cameraList->getCamera(0)->getDescriptorSet(), 1, *m_particlesPipeline);
	m_commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 2, *m_particlesPipeline);
	m_commandBuffer->draw(m_particlesUpdatePass->getParticleCount() * 6, 1, 0, 0);

	/* UI */
	m_commandBuffer->bindPipeline(m_userInterfacePipeline.get());
	m_commandBuffer->bindDescriptorSet(m_userInterfaceDescriptorSet.get(), 0, *m_userInterfacePipeline);

	m_fullscreenRect->draw(*m_commandBuffer, RenderMeshList::NO_CAMERA_IDX);

	m_commandBuffer->endRenderPass();

	DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void ForwardPass::submit(const SubmitContext& context)
{
	std::vector waitSemaphores{ m_contaminationUpdateSemaphore, context.swapChainImageAvailableSemaphore, context.userInterfaceImageAvailableSemaphore };
	if (m_particlesUpdatePass->getParticleCount() > 0)
		waitSemaphores.push_back(m_particlesUpdatePass->getSemaphore());
	const std::vector<const Semaphore*> signalSemaphores{ m_semaphore.get() };
	m_commandBuffer->submit(waitSemaphores, signalSemaphores, context.frameFence);

	bool anyShaderModified = m_userInterfaceVertexShaderParser->compileIfFileHasBeenModified();
	if (m_userInterfaceFragmentShaderParser->compileIfFileHasBeenModified())
		anyShaderModified = true;
	if (m_particlesVertexShaderParser->compileIfFileHasBeenModified())
		anyShaderModified = true;
	if (m_particlesFragmentShaderParser->compileIfFileHasBeenModified())
		anyShaderModified = true;

	if (anyShaderModified)
	{
		context.graphicAPIManager->waitIdle();
		createPipelines();
	}
}

void ForwardPass::updateLights(const Wolf::ResourceNonOwner<LightManager>& lightManager) const
{
	PointLightsUBData pointLightsUBData{};

	uint32_t pointLightIdx = 0;
	for (const LightManager::PointLightInfo& pointLightInfo : lightManager->getCurrentLights())
	{
		pointLightsUBData.lights[pointLightIdx].lightPos = glm::vec4(pointLightInfo.worldPos, 1.0f);
		pointLightsUBData.lights[pointLightIdx].lightColor = glm::vec4(pointLightInfo.color * pointLightInfo.intensity * 0.01f, 1.0f);

		pointLightIdx++;

		if (pointLightIdx == MAX_POINT_LIGHTS)
		{
			Debug::sendMessageOnce("Max point lights reached, next will be ignored", Debug::Severity::WARNING, this);
			break;
		}
	}
	pointLightsUBData.count = pointLightIdx;

	m_pointLightsUniformBuffer->transferCPUMemory(&pointLightsUBData, sizeof(PointLightsUBData));
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
	m_depthImage.reset(Image::createImage(depthImageCreateInfo));

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
		m_frameBuffers[i].reset(FrameBuffer::createFrameBuffer(*m_renderPass, { colorAttachment,  depthAttachment }));
	}
}

void ForwardPass::createPipelines()
{
	// Particles
	{
		RenderingPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.renderPass = m_renderPass.get();

		// Programming stages
		pipelineCreateInfo.shaderCreateInfos.resize(2);
		m_particlesVertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
		pipelineCreateInfo.shaderCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		m_particlesFragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
		pipelineCreateInfo.shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		// Viewport
		pipelineCreateInfo.extent = { m_renderWidth, m_renderHeight };

		// Rasterization
		pipelineCreateInfo.cullMode = VK_CULL_MODE_NONE;

		// Resources
		pipelineCreateInfo.descriptorSetLayouts = { m_particlesDescriptorSetLayout.get(), GraphicCameraInterface::getDescriptorSetLayout(), MaterialsGPUManager::getDescriptorSetLayout() };

		// Color Blend
		pipelineCreateInfo.blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ALPHA };

		// Dynamic state
		pipelineCreateInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		m_particlesPipeline.reset(Pipeline::createRenderingPipeline(pipelineCreateInfo));
	}

	// UI
	{
		RenderingPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.renderPass = m_renderPass.get();

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
		pipelineCreateInfo.descriptorSetLayouts = { m_userInterfaceDescriptorSetLayout.get() };

		// Color Blend
		pipelineCreateInfo.blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ALPHA };

		m_userInterfacePipeline.reset(Pipeline::createRenderingPipeline(pipelineCreateInfo));
	}
}
