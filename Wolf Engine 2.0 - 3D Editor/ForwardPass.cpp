#include "ForwardPass.h"

#include <DebugMarker.h>
#include <DescriptorSet.h>
#include <DescriptorSetGenerator.h>
#include <GraphicCameraInterface.h>
#include <Image.h>
#include <ProfilerCommon.h>

#include "CommonLayouts.h"
#include "ContaminationUpdatePass.h"
#include "GameContext.h"
#include "EditorModelInterface.h"
#include "LightManager.h"
#include "Vertex2DTextured.h"

void ForwardPass::initializeResources(const Wolf::InitializationContext& context)
{
	Wolf::Attachment color = setupColorAttachment(context);
	Wolf::Attachment depth = setupDepthAttachment(context);

	m_renderPass.reset(Wolf::RenderPass::createRenderPass({ color, depth }));
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false /* isTransient */));
	initializeFramesBuffers(context, color, depth);
	
	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT));

	// Shared resources
	{
		m_displayOptionsUniformBuffer.reset(Wolf::Buffer::createBuffer(sizeof(DisplayOptionsUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

		m_commonDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
		m_commonDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_commonDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		Wolf::DescriptorSetGenerator descriptorSetGenerator(m_commonDescriptorSetLayoutGenerator.getDescriptorLayouts());
		descriptorSetGenerator.setBuffer(0, *m_displayOptionsUniformBuffer);

		m_commonDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_commonDescriptorSetLayout));
		m_commonDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
	}

	// Particles resources
	{
		m_particlesDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::VERTEX,                                       0); // particles buffer
		m_particlesDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::FRAGMENT | Wolf::ShaderStageFlagBits::VERTEX, 1); // emitters buffer
		m_particlesDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::FRAGMENT | Wolf::ShaderStageFlagBits::VERTEX, 2); // noise buffer
		m_particlesDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_particlesDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		Wolf::DescriptorSetGenerator descriptorSetGenerator(m_particlesDescriptorSetLayoutGenerator.getDescriptorLayouts());
		descriptorSetGenerator.setBuffer(0, m_particlesUpdatePass->getParticleBuffer());
		descriptorSetGenerator.setBuffer(1, m_particlesUpdatePass->getEmittersBuffer());
		descriptorSetGenerator.setBuffer(2, m_particlesUpdatePass->getNoiseBuffer());

		m_particlesDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_particlesDescriptorSetLayout));
		m_particlesDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

		m_particlesVertexShaderParser.reset(new Wolf::ShaderParser("Shaders/particles/render.vert", {}, 1));
		Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
		m_shadowMaskPass->addShaderCode(shaderCodeToAdd, SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES);
		m_particlesFragmentShaderParser.reset(new Wolf::ShaderParser("Shaders/particles/render.frag", {}, 1, 2, 3, Wolf::ShaderParser::MaterialFetchProcedure(),
		                                                             shaderCodeToAdd));
	}

	// UI resources
	{
		m_userInterfaceDescriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
		m_userInterfaceDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_userInterfaceDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		Wolf::DescriptorSetGenerator descriptorSetGenerator(m_userInterfaceDescriptorSetLayoutGenerator.getDescriptorLayouts());
		m_userInterfaceSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 11, VK_FILTER_LINEAR));
		descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, context.userInterfaceImage->getDefaultImageView(), *m_userInterfaceSampler);

		m_userInterfaceDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_userInterfaceDescriptorSetLayout));
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

		m_fullscreenRect.reset(new Wolf::Mesh(vertices, indices));

		m_userInterfaceVertexShaderParser.reset(new Wolf::ShaderParser("Shaders/UI/UI.vert"));
		m_userInterfaceFragmentShaderParser.reset(new Wolf::ShaderParser("Shaders/UI/UI.frag"));
	}

	m_renderWidth = context.swapChainWidth;
	m_renderHeight = context.swapChainHeight;
	createPipelines();
}

void ForwardPass::resize(const Wolf::InitializationContext& context)
{
	m_renderPass->setExtent({ context.swapChainWidth, context.swapChainHeight });

	Wolf::Attachment color = setupColorAttachment(context);
	Wolf::Attachment depth = setupDepthAttachment(context);
	initializeFramesBuffers(context, color, depth);

	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_userInterfaceDescriptorSetLayoutGenerator.getDescriptorLayouts());
	descriptorSetGenerator.setCombinedImageSampler(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, context.userInterfaceImage->getDefaultImageView(), *m_userInterfaceSampler);
	m_userInterfaceDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	m_renderWidth = context.swapChainWidth;
	m_renderHeight = context.swapChainHeight;
	createPipelines(); // may be overkill but resets the viewport for UI
}

void ForwardPass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	/* Command buffer record */
	const GameContext* gameContext = static_cast<const GameContext*>(context.gameContext);
	const uint32_t frameBufferIdx = context.swapChainImageIdx;

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Forward pass");

	std::vector<Wolf::ClearValue> clearValues(2);
	clearValues[0] = { 0.1f, 0.1f, 0.1f, 1.0f };
	clearValues[1] = { 1.0f };
	m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffers[frameBufferIdx], clearValues);

	/* Shared resources */
	DisplayOptionsUBData displayOptions{};
	displayOptions.displayType = static_cast<uint32_t>(gameContext->displayType);
	m_displayOptionsUniformBuffer->transferCPUMemory(&displayOptions, sizeof(displayOptions), 0);

	const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();
	m_commandBuffer->setViewport(renderViewport);

	Wolf::DescriptorSetBindInfo commonDescriptorSetBindInfo(m_commonDescriptorSet.createConstNonOwnerResource(), m_commonDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_FORWARD_COMMON);

	std::vector<Wolf::RenderMeshList::AdditionalDescriptorSet> descriptorSetBindInfos;
	descriptorSetBindInfos.emplace_back(commonDescriptorSetBindInfo, 0);
	descriptorSetBindInfos.emplace_back(m_shadowMaskPass->getMaskDescriptorSetToBind(), AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO);

	context.renderMeshList->draw(context, *m_commandBuffer, m_renderPass.get(), CommonPipelineIndices::PIPELINE_IDX_FORWARD, CommonCameraIndices::CAMERA_IDX_MAIN, descriptorSetBindInfos);

	/* Particles */
	if (uint32_t particleCount = m_particlesUpdatePass->getParticleCount())
	{
		m_commandBuffer->bindPipeline(m_particlesPipeline.get());
		m_commandBuffer->setViewport(renderViewport);
		m_commandBuffer->bindDescriptorSet(m_particlesDescriptorSet.get(), 0, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(context.cameraList->getCamera(0)->getDescriptorSet(), 1, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 2, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(context.lightManager->getDescriptorSet().createConstNonOwnerResource(), 3, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(m_shadowMaskPass->getShadowComputeDescriptorSetToBind(SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES).getDescriptorSet(), SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(m_commonDescriptorSet.createConstNonOwnerResource(), 5, *m_particlesPipeline);
		m_commandBuffer->draw(particleCount * 6, 1, 0, 0);
	}

	/* UI */
	m_commandBuffer->bindPipeline(m_userInterfacePipeline.get());
	m_commandBuffer->bindDescriptorSet(m_userInterfaceDescriptorSet.get(), 0, *m_userInterfacePipeline);

	m_fullscreenRect->draw(*m_commandBuffer, Wolf::RenderMeshList::NO_CAMERA_IDX);

	m_commandBuffer->endRenderPass();

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void ForwardPass::submit(const Wolf::SubmitContext& context)
{
	std::vector waitSemaphores{ context.swapChainImageAvailableSemaphore, context.userInterfaceImageAvailableSemaphore };
	if (m_contaminationUpdatePass->wasEnabledThisFrame())
		waitSemaphores.push_back(m_contaminationUpdatePass->getSemaphore());
	if (m_particlesUpdatePass->getParticleCount() > 0)
		waitSemaphores.push_back(m_particlesUpdatePass->getSemaphore());
	if (m_shadowMaskPass->wasEnabledThisFrame())
		waitSemaphores.push_back(m_shadowMaskPass->getSemaphore());
	else
		waitSemaphores.push_back(m_preDepthPass->getSemaphore());
	const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };
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

Wolf::Attachment ForwardPass::setupColorAttachment(const Wolf::InitializationContext& context)
{
	return Wolf::Attachment({ context.swapChainWidth, context.swapChainHeight }, context.swapChainFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_STORE_OP_STORE, Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT,
		nullptr);
}

Wolf::Attachment ForwardPass::setupDepthAttachment(const Wolf::InitializationContext& context)
{
	Wolf::CreateImageInfo depthImageCreateInfo;
	depthImageCreateInfo.format = context.depthFormat;
	depthImageCreateInfo.extent.width = context.swapChainWidth;
	depthImageCreateInfo.extent.height = context.swapChainHeight;
	depthImageCreateInfo.extent.depth = 1;
	depthImageCreateInfo.mipLevelCount = 1;
	depthImageCreateInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthImageCreateInfo.usage = Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT;
	m_depthImage.reset(Wolf::Image::createImage(depthImageCreateInfo));

	return Wolf::Attachment({ context.swapChainWidth, context.swapChainHeight }, context.depthFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_STORE,
	                        Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT, m_depthImage->getDefaultImageView());
}

void ForwardPass::initializeFramesBuffers(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment)
{
	m_frameBuffers.clear();
	m_frameBuffers.resize(context.swapChainImageCount);
	for (uint32_t i = 0; i < context.swapChainImageCount; ++i)
	{
		colorAttachment.imageView = context.swapChainImages[i]->getDefaultImageView();
		m_frameBuffers[i].reset(Wolf::FrameBuffer::createFrameBuffer(*m_renderPass, { colorAttachment,  depthAttachment }));
	}
}

void ForwardPass::createPipelines()
{
	// Particles
	{
		Wolf::RenderingPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.renderPass = m_renderPass.get();

		// Programming stages
		pipelineCreateInfo.shaderCreateInfos.resize(2);
		m_particlesVertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
		pipelineCreateInfo.shaderCreateInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;
		m_particlesFragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
		pipelineCreateInfo.shaderCreateInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

		// Viewport
		pipelineCreateInfo.extent = { m_renderWidth, m_renderHeight };

		// Rasterization
		pipelineCreateInfo.cullMode = VK_CULL_MODE_NONE;

		// Resources
		pipelineCreateInfo.descriptorSetLayouts = 
		{
			m_particlesDescriptorSetLayout.get(),
			Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource(),
			Wolf::MaterialsGPUManager::getDescriptorSetLayout().createConstNonOwnerResource(),
			Wolf::LightManager::getDescriptorSetLayout().createConstNonOwnerResource(),
			m_shadowMaskPass->getShadowComputeDescriptorSetToBind(SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES).getDescriptorSetLayout(),
			m_commonDescriptorSetLayout.createConstNonOwnerResource()
		};

		// Color Blend
		pipelineCreateInfo.blendModes = {Wolf::RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ADD };

		// Depth testing
		pipelineCreateInfo.enableDepthWrite = false;

		// Dynamic state
		pipelineCreateInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		m_particlesPipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));
	}

	// UI
	{
		Wolf::RenderingPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.renderPass = m_renderPass.get();

		// Programming stages
		pipelineCreateInfo.shaderCreateInfos.resize(2);
		m_userInterfaceVertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
		pipelineCreateInfo.shaderCreateInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;
		m_userInterfaceFragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
		pipelineCreateInfo.shaderCreateInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

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
		pipelineCreateInfo.blendModes = {Wolf::RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ALPHA };

		m_userInterfacePipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));
	}
}
