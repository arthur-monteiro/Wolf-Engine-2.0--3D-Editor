#include "ForwardPass.h"

#include <DebugMarker.h>
#include <DescriptorSet.h>
#include <DescriptorSetGenerator.h>
#include <GraphicCameraInterface.h>
#include <Image.h>
#include <ProfilerCommon.h>
#include <stb_image_write.h>

#include "CommonLayouts.h"
#include "ContaminationUpdatePass.h"
#include "GameContext.h"
#include "EditorModelInterface.h"
#include "LightManager.h"
#include "Vertex2DTextured.h"
#include "VoxelGlobalIlluminationPass.h"

void ForwardPass::initializeResources(const Wolf::InitializationContext& context)
{
	createOutputImage(context);

	Wolf::Attachment color = setupColorAttachment(context);
	Wolf::Attachment depth = setupDepthAttachment(context);

	m_renderPass.reset(Wolf::RenderPass::createRenderPass({ color, depth }));
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false /* isTransient */));
	initializeFramesBuffer(context, color, depth);

	createSemaphores(context, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, false);

	// Shared resources
	{
		m_displayOptionsUniformBuffer.reset(new Wolf::UniformBuffer(sizeof(DisplayOptionsUBData)));

		m_commonDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
		m_commonDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_commonDescriptorSetLayoutGenerator.getDescriptorLayouts()));

		Wolf::DescriptorSetGenerator descriptorSetGenerator(m_commonDescriptorSetLayoutGenerator.getDescriptorLayouts());
		descriptorSetGenerator.setUniformBuffer(0, *m_displayOptionsUniformBuffer);

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
	}

	// Full screen rect resources
	{
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
	initializeFramesBuffer(context, color, depth);

	m_renderWidth = context.swapChainWidth;
	m_renderHeight = context.swapChainHeight;
	createPipelines(); // may be overkill but resets the viewport for UI // TODO: still needed?
}

void ForwardPass::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	/* Command buffer record */
	const GameContext* gameContext = static_cast<const GameContext*>(context.gameContext);

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Forward pass");

	std::vector<Wolf::ClearValue> clearValues(1);
	clearValues[0] = { CLEAR_VALUE, CLEAR_VALUE, CLEAR_VALUE, 1.0f };
	m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffer, clearValues);

	/* Shared resources */
	DisplayOptionsUBData displayOptions{};
	displayOptions.displayType = static_cast<uint32_t>(gameContext->displayType);
	displayOptions.enableTrilinearVoxelGI = m_enableTrilinearVoxelGI ? 1 : 0;
	displayOptions.exposure = m_exposure;
	m_displayOptionsUniformBuffer->transferCPUMemory(&displayOptions, sizeof(displayOptions), 0);

	const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();
	m_commandBuffer->setViewport(renderViewport);

	m_skyBoxManager->drawSkyBox(*m_commandBuffer, *m_renderPass, context);

	{
		PROFILE_SCOPED("Draw record")

		Wolf::DescriptorSetBindInfo commonDescriptorSetBindInfo(m_commonDescriptorSet.createConstNonOwnerResource(), m_commonDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_PASS_INFO);

		std::vector<Wolf::RenderMeshList::AdditionalDescriptorSet> descriptorSetBindInfos;
		descriptorSetBindInfos.emplace_back(commonDescriptorSetBindInfo, 0);
		descriptorSetBindInfos.emplace_back(m_shadowMaskPass->getMaskDescriptorSetToBind(), AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO);
		descriptorSetBindInfos.emplace_back(m_globalIrradiancePass->getDescriptorSetToBind(), AdditionalDescriptorSetsMaskBits::GLOBAL_IRRADIANCE_SHADOW_MASK_INFO);

		std::vector<Wolf::PipelineSet::ShaderCodeToAddForStage> shadersCodeToAdd(1);
		shadersCodeToAdd[0].stage = Wolf::ShaderStageFlagBits::FRAGMENT;
		m_globalIrradiancePass->addShaderCode(shadersCodeToAdd[0].shaderCodeToAdd, DescriptorSetSlots::DESCRIPTOR_SET_SLOT_GLOBAL_IRRADIANCE_INFO);
		shadersCodeToAdd[0].requiredMask = AdditionalDescriptorSetsMaskBits::GLOBAL_IRRADIANCE_SHADOW_MASK_INFO;

		context.renderMeshList->draw(context, *m_commandBuffer, m_renderPass.get(), CommonPipelineIndices::PIPELINE_IDX_FORWARD, CommonCameraIndices::CAMERA_IDX_MAIN, descriptorSetBindInfos, shadersCodeToAdd);
	}

	/* Particles */
	if (uint32_t particleCount = m_particlesUpdatePass->getParticleCount())
	{
		m_commandBuffer->bindPipeline(m_particlesPipeline.get());
		m_commandBuffer->setViewport(renderViewport);
		m_commandBuffer->bindDescriptorSet(m_particlesDescriptorSet.get(), 0, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(context.cameraList->getCamera(0)->getDescriptorSet(), 1, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 2, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(context.lightManager->getDescriptorSet().createConstNonOwnerResource(), 3, *m_particlesPipeline);
		m_commandBuffer->bindDescriptorSet(m_commonDescriptorSet.createConstNonOwnerResource(), 4, *m_particlesPipeline);
		if (m_shadowMaskPass->hasDescriptorSetToBindForCompute())
		{
			m_commandBuffer->bindDescriptorSet(m_shadowMaskPass->getShadowComputeDescriptorSetToBind(SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES).getDescriptorSet(), SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES,
				*m_particlesPipeline);
		}
		m_commandBuffer->draw(particleCount * 6, 1, 0, 0);
	}

	/* UI and draw rects */
	m_commandBuffer->setViewport({ 0, 0, static_cast<float>(context.swapchainImage->getExtent().width), static_cast<float>(context.swapchainImage->getExtent().height), 0, 1.0f });
	bool isRayTracedDebugEnabled = gameContext->displayType == GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_ALBEDO || gameContext->displayType == GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_INSTANCE_ID ||
		gameContext->displayType == GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_PRIMITIVE_ID;
	if (isRayTracedDebugEnabled && m_rayTracedWorldDebugPass && m_rayTracedWorldDebugPass->wasEnabledThisFrame())
	{
		m_rayTracedWorldDebugPass->bindInfoToDrawOutput(*m_commandBuffer, m_renderPass.get());
		m_fullscreenRect->draw(*m_commandBuffer, Wolf::RenderMeshList::NO_CAMERA_IDX);
	}
	else if (gameContext->displayType == GameContext::DisplayType::PATH_TRACING && m_pathTracingPass && m_pathTracingPass->wasEnabledThisFrame())
	{
		m_pathTracingPass->bindInfoToDrawOutput(*m_commandBuffer, m_renderPass.get());
		m_fullscreenRect->draw(*m_commandBuffer, Wolf::RenderMeshList::NO_CAMERA_IDX);
	}

	m_commandBuffer->endRenderPass();

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void ForwardPass::submit(const Wolf::SubmitContext& context)
{
	std::vector<const Wolf::Semaphore*> waitSemaphores{ };
	if (m_contaminationUpdatePass->wasEnabledThisFrame())
		waitSemaphores.push_back(m_contaminationUpdatePass->getSemaphore(context.swapChainImageIndex));
	if (m_particlesUpdatePass->getParticleCount() > 0)
		waitSemaphores.push_back(m_particlesUpdatePass->getSemaphore(context.swapChainImageIndex));
	if (m_shadowMaskPass->wasEnabledThisFrame())
		waitSemaphores.push_back(m_shadowMaskPass->getSemaphore());
	else
		waitSemaphores.push_back(m_preDepthPass->getSemaphore(context.swapChainImageIndex));
	if (m_rayTracedWorldDebugPass && m_rayTracedWorldDebugPass->wasEnabledThisFrame())
		waitSemaphores.push_back(m_rayTracedWorldDebugPass->getSemaphore(context.swapChainImageIndex));
	if (m_pathTracingPass && m_pathTracingPass->wasEnabledThisFrame())
		waitSemaphores.push_back(m_pathTracingPass->getSemaphore(context.swapChainImageIndex));
	else if (m_computeSkyCubeMapPass->wasEnabledThisFrame())
		waitSemaphores.push_back({ m_computeSkyCubeMapPass->getSemaphore(context.swapChainImageIndex) });
	if (m_globalIrradiancePass->wasEnabledThisFrame())
		waitSemaphores.push_back(m_globalIrradiancePass->getSemaphore());

	const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
	m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);

	bool anyShaderModified = m_particlesVertexShaderParser->compileIfFileHasBeenModified();
	if (m_particlesFragmentShaderParser->compileIfFileHasBeenModified())
		anyShaderModified = true;

	if (anyShaderModified)
	{
		context.graphicAPIManager->waitIdle();
		createPipelines();
	}
}

void ForwardPass::setShadowMaskPass(const Wolf::ResourceNonOwner<ShadowMaskPassInterface>& shadowMaskPassInterface, Wolf::GraphicAPIManager* graphicApiManager)
{
	if (m_shadowMaskPass != shadowMaskPassInterface)
	{
		m_shadowMaskPass = shadowMaskPassInterface;

		Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
		m_shadowMaskPass->addComputeShadowsShaderCode(shaderCodeToAdd, SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES);
		m_particlesFragmentShaderParser.reset(new Wolf::ShaderParser("Shaders/particles/render.frag", {}, 1, 2, 3, Wolf::ShaderParser::MaterialFetchProcedure(),
																	 shaderCodeToAdd));

		graphicApiManager->waitIdle();
		createPipelines();
	}
}

void ForwardPass::createOutputImage(const Wolf::InitializationContext& context)
{
	Wolf::CreateImageInfo createInfo{};
	createInfo.extent = { context.swapChainWidth, context.swapChainHeight, 1 };
	createInfo.usage = Wolf::ImageUsageFlagBits::STORAGE | Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT;
	createInfo.format = Wolf::Format::R16G16B16A16_SFLOAT;
	createInfo.mipLevelCount = 1;
	m_outputImage.reset(Wolf::Image::createImage(createInfo));
	m_outputImage->setImageLayout({ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, 0, 1,
		VK_IMAGE_LAYOUT_UNDEFINED });
}

Wolf::Attachment ForwardPass::setupColorAttachment(const Wolf::InitializationContext& context)
{
	return Wolf::Attachment({ context.swapChainWidth, context.swapChainHeight }, Wolf::Format::R16G16B16A16_SFLOAT, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_GENERAL, Wolf::AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT,
		m_outputImage->getDefaultImageView());
}

Wolf::Attachment ForwardPass::setupDepthAttachment(const Wolf::InitializationContext& context)
{
	Wolf::Attachment attachment({ context.swapChainWidth, context.swapChainHeight }, context.depthFormat, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		Wolf::AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT, m_preDepthPass->getOutput()->getDefaultImageView());
	attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;;
	attachment.loadOperation = Wolf::AttachmentLoadOp::LOAD;

	return attachment;
}

void ForwardPass::initializeFramesBuffer(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment)
{
	m_frameBuffer.reset(Wolf::FrameBuffer::createFrameBuffer(*m_renderPass, { colorAttachment,  depthAttachment }));
}

void ForwardPass::createPipelines()
{
	// Particles
	if (m_particlesFragmentShaderParser)
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
			m_commonDescriptorSetLayout.createConstNonOwnerResource()
		};

		if (m_shadowMaskPass->hasDescriptorSetToBindForCompute())
		{
			pipelineCreateInfo.descriptorSetLayouts.push_back(m_shadowMaskPass->getShadowComputeDescriptorSetToBind(SHADOW_COMPUTE_DESCRIPTOR_SET_SLOT_FOR_PARTICLES).getDescriptorSetLayout());
		}

		// Color Blend
		pipelineCreateInfo.blendModes = {Wolf::RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ADD };

		// Depth testing
		pipelineCreateInfo.enableDepthWrite = false;

		// Dynamic state
		pipelineCreateInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

		m_particlesPipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));
	}
}
