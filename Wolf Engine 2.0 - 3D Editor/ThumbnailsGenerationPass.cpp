#include "ThumbnailsGenerationPass.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <CameraList.h>
#include <DebugMarker.h>
#include <FrameBuffer.h>
#include <GraphicCameraInterface.h>
#include <Image.h>
#include <ModelLoader.h>
#include <Pipeline.h>
#include <RenderPass.h>

#include "AnimationHelper.h"
#include "CommonLayouts.h"

class AnimatedModel;

ThumbnailsGenerationPass::ThumbnailsGenerationPass()
{
	m_camera.reset(new Wolf::FirstPersonCamera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f, 1.0f));
}

void ThumbnailsGenerationPass::initializeResources(const Wolf::InitializationContext& context)
{
	Wolf::CreateImageInfo createRenderTargetInfo;
	createRenderTargetInfo.extent = { OUTPUT_SIZE, OUTPUT_SIZE, 1 };
	createRenderTargetInfo.usage = Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT | Wolf::ImageUsageFlagBits::TRANSFER_SRC;
	createRenderTargetInfo.format = OUTPUT_FORMAT;
	createRenderTargetInfo.mipLevelCount = 1;
	m_renderTargetImage.reset(Wolf::Image::createImage(createRenderTargetInfo));
	Wolf::Attachment color = Wolf::Attachment({ OUTPUT_SIZE, OUTPUT_SIZE }, OUTPUT_FORMAT, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Wolf::AttachmentStoreOp::STORE,
		Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT, m_renderTargetImage->getDefaultImageView());

	Wolf::CreateImageInfo depthImageCreateInfo;
	depthImageCreateInfo.format = context.depthFormat;
	depthImageCreateInfo.extent.width = OUTPUT_SIZE;
	depthImageCreateInfo.extent.height = OUTPUT_SIZE;
	depthImageCreateInfo.extent.depth = 1;
	depthImageCreateInfo.mipLevelCount = 1;
	depthImageCreateInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthImageCreateInfo.usage = Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT;
	m_depthImage.reset(Wolf::Image::createImage(depthImageCreateInfo));
	Wolf::Attachment depth = Wolf::Attachment({ OUTPUT_SIZE, OUTPUT_SIZE }, context.depthFormat, Wolf::SAMPLE_COUNT_1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, Wolf::AttachmentStoreOp::STORE,
		Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT, m_depthImage->getDefaultImageView());

	Wolf::CreateImageInfo createCopyInfo;
	createCopyInfo.extent = { OUTPUT_SIZE, OUTPUT_SIZE, 1 };
	createCopyInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST;
	createCopyInfo.format = OUTPUT_FORMAT;
	createCopyInfo.mipLevelCount = 1;
	createCopyInfo.imageTiling = VK_IMAGE_TILING_LINEAR;
	createCopyInfo.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	m_copyImage.reset(Wolf::Image::createImage(createCopyInfo));
	m_copyImage->setImageLayout({ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED });

	m_renderPass.reset(Wolf::RenderPass::createRenderPass({ color, depth }));
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::GRAPHIC, false));
	m_frameBuffer.reset(Wolf::FrameBuffer::createFrameBuffer(*m_renderPass, { color, depth }));

	Wolf::RenderingPipelineCreateInfo pipelineCreateInfo;
	pipelineCreateInfo.renderPass = m_renderPass.get();

	// Programming stages
	m_staticVertexShaderParser.reset(new Wolf::ShaderParser("Shaders/thumbnailsGeneration/staticShader.vert", {}, 0));
	m_fragmentShaderParser.reset(new Wolf::ShaderParser("Shaders/thumbnailsGeneration/defaultShader.frag", {}, 0 /* needed for virtual texture */, 1));

	pipelineCreateInfo.shaderCreateInfos.resize(2);
	m_staticVertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
	pipelineCreateInfo.shaderCreateInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;
	m_fragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
	pipelineCreateInfo.shaderCreateInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

	// IA
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	Wolf::Vertex3D::getAttributeDescriptions(attributeDescriptions, 0);
	pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

	std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
	bindingDescriptions[0] = {};
	Wolf::Vertex3D::getBindingDescription(bindingDescriptions[0], 0);
	pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

	// Viewport
	pipelineCreateInfo.extent = { OUTPUT_SIZE, OUTPUT_SIZE };

	// Resources
	pipelineCreateInfo.descriptorSetLayouts = { Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource(), Wolf::MaterialsGPUManager::getDescriptorSetLayout().createConstNonOwnerResource() };

	// Color Blend
	pipelineCreateInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ALPHA };

	m_staticPipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));

	/* Animated pipeline */
	// Programming stages
	m_animatedVertexShaderParser.reset(new Wolf::ShaderParser("Shaders/thumbnailsGeneration/animatedShader.vert", {}, 0));
	m_animatedVertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);

	// IA
	attributeDescriptions.clear();
	Wolf::SkeletonVertex::getAttributeDescriptions(attributeDescriptions, 0);
	pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

	bindingDescriptions.resize(1);
	bindingDescriptions[0] = {};
	Wolf::SkeletonVertex::getBindingDescription(bindingDescriptions[0], 0);
	pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

	// Resources
	m_animationDescriptorSetGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::VERTEX, 0);
	m_animationDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_animationDescriptorSetGenerator.getDescriptorLayouts()));
	pipelineCreateInfo.descriptorSetLayouts.emplace_back(m_animationDescriptorSetLayout.createConstNonOwnerResource());

	m_animatedPipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));

	m_animationDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_animationDescriptorSetLayout));
	Wolf::DescriptorSetGenerator descriptorSetGenerator(m_animationDescriptorSetGenerator.getDescriptorLayouts());
	static constexpr uint32_t MAX_BONE_COUNT = 1024;
	m_bonesBuffer.reset(Wolf::Buffer::createBuffer(MAX_BONE_COUNT * sizeof(glm::mat4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	descriptorSetGenerator.setBuffer(0, *m_bonesBuffer);
	m_animationDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void ThumbnailsGenerationPass::resize(const Wolf::InitializationContext& context)
{
	// Nothing to do
}

uint32_t ThumbnailsGenerationPass::computeTotalImageToDraw(const Request& request)
{
	float maxTimer;
	findMaxTimer(request.m_modelData->animationData->rootBones.data(), maxTimer);
	return std::max(static_cast<uint32_t>((maxTimer * 1000.0f) / DELAY_BETWEEN_ICON_FRAMES_MS), 1u);
}

void ThumbnailsGenerationPass::record(const Wolf::RecordContext& context)
{
	if (!m_currentRequests.empty())
	{
		Request& previousRequest = m_currentRequests.front();

		const void* outputBytes = m_copyImage->map();
		VkSubresourceLayout copyResourceLayout;
		m_copyImage->getResourceLayout(copyResourceLayout);

		if (!previousRequest.m_modelData->animationData)
		{
			stbi_write_png(previousRequest.m_outputFullFilepath.c_str(), OUTPUT_SIZE, OUTPUT_SIZE, 4, outputBytes, static_cast<int>(copyResourceLayout.rowPitch));
		}
		else
		{
			uint32_t totalImagesToDraw = computeTotalImageToDraw(previousRequest);

			if (previousRequest.m_imageLeftToDraw == totalImagesToDraw - 1)
			{
				static constexpr int quality = 10;
				static constexpr bool useGlobalColorMap = true;
				static constexpr int loop = 0;
				static constexpr int preAllocSize = useGlobalColorMap ? OUTPUT_SIZE * OUTPUT_SIZE * 3 * 3 : OUTPUT_SIZE * OUTPUT_SIZE * 3;

				if (!m_gifEncoder.open(previousRequest.m_outputFullFilepath, OUTPUT_SIZE, OUTPUT_SIZE, quality, useGlobalColorMap, loop, preAllocSize))
				{
					Wolf::Debug::sendError("Error when opening gif file");
				}
			}

			std::vector<uint8_t> data(static_cast<size_t>(OUTPUT_SIZE) * OUTPUT_SIZE * 4);
			for (size_t i = 0; i < OUTPUT_SIZE; ++i)
			{
				memcpy(&data[i * OUTPUT_SIZE * 4], &static_cast<const uint8_t*>(outputBytes)[i * copyResourceLayout.rowPitch], static_cast<size_t>(OUTPUT_SIZE) * 4);
			}
			m_gifEncoder.push(GifEncoder::PIXEL_FORMAT_RGBA, data.data(), OUTPUT_SIZE, OUTPUT_SIZE, static_cast<uint32_t>(DELAY_BETWEEN_ICON_FRAMES_MS) / 10);

			if (previousRequest.m_imageLeftToDraw == 0)
			{
				if (!m_gifEncoder.close())
				{
					Wolf::Debug::sendError("Error when closing gif file");
				}
			}
		}

		if (previousRequest.m_imageLeftToDraw == 0)
		{
			previousRequest.m_onGeneratedCallback();
		}

		m_copyImage->unmap();
		m_currentRequests.pop_front();
	}

	if (m_pendingRequests.empty())
	{
		m_drawRecordedThisFrame = false;
		return;
	}

	Request& request = m_pendingRequests.front();

	if (request.m_imageLeftToDraw == 0)
	{
		Wolf::Debug::sendError("Image left to draw should not be zero here");
		request.m_imageLeftToDraw = 1;
	}

	if (request.m_modelData->animationData)
	{
		uint32_t totalImagesToDraw = computeTotalImageToDraw(request);

		uint32_t boneCount = request.m_modelData->animationData->boneCount;
		std::vector<BoneInfoGPU> bonesInfoGPU(boneCount);
		std::vector<BoneInfoCPU> bonesInfoCPU(boneCount);

		computeBonesInfo(request.m_modelData->animationData->rootBones.data(), glm::mat4(1.0f), (static_cast<float>(totalImagesToDraw - request.m_imageLeftToDraw) * DELAY_BETWEEN_ICON_FRAMES_MS) / 1000.0f, 
			glm::mat4(1.0f), bonesInfoGPU, bonesInfoCPU);

		m_bonesBuffer->transferCPUMemory(bonesInfoGPU.data(), static_cast<uint32_t>(bonesInfoGPU.size() * sizeof(BoneInfoGPU)));
	}

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Thumbnails generation pass");

	std::vector<Wolf::ClearValue> clearValues(2);
	clearValues[0] = { 0.1f, 0.1f, 0.1f, 1.0f };
	clearValues[1] = { 1.0f };
	m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffer, clearValues);

	Wolf::Pipeline* pipeline;
	if (request.m_modelData->animationData)
	{
		pipeline = m_animatedPipeline.get();
	}
	else
	{
		pipeline = m_staticPipeline.get();
	}

	m_commandBuffer->bindPipeline(pipeline);
	m_commandBuffer->bindDescriptorSet(context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_THUMBNAIL_GENERATION)->getDescriptorSet(), 0, *pipeline);
	m_commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 1, *pipeline);
	if (request.m_modelData->animationData)
	{
		m_commandBuffer->bindDescriptorSet(m_animationDescriptorSet.createConstNonOwnerResource(), 2, *pipeline);
	}
	request.m_modelData->mesh->draw(*m_commandBuffer, 0);

	m_commandBuffer->endRenderPass();

	VkImageCopy copyRegion{};

	copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.srcSubresource.baseArrayLayer = 0;
	copyRegion.srcSubresource.mipLevel = 0;
	copyRegion.srcSubresource.layerCount = 1;
	copyRegion.srcOffset = { 0, 0, 0 };

	copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.dstSubresource.baseArrayLayer = 0;
	copyRegion.dstSubresource.mipLevel = 0;
	copyRegion.dstSubresource.layerCount = 1;
	copyRegion.dstOffset = { 0, 0, 0 };

	Wolf::Extent3D extent = m_copyImage->getExtent();
	copyRegion.extent = { extent.width, extent.height, extent.depth };

	m_renderTargetImage->setImageLayoutWithoutOperation(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); // at this point, render pass should have set final layout
	m_copyImage->recordCopyGPUImage(*m_renderTargetImage, copyRegion, *m_commandBuffer);

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();

	m_drawRecordedThisFrame = true;

	request.m_imageLeftToDraw--;
	m_currentRequests.push_back(request);
	if (request.m_imageLeftToDraw == 0)
	{
		m_pendingRequests.pop_front();
	}
}

void ThumbnailsGenerationPass::submit(const Wolf::SubmitContext& context)
{
	if (m_drawRecordedThisFrame)
	{
		std::vector<const Wolf::Semaphore*> waitSemaphores{ };
		const std::vector<const Wolf::Semaphore*> signalSemaphores{ /*m_semaphore.get()*/ };
		m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
	}
}

void ThumbnailsGenerationPass::addCameraForThisFrame(Wolf::CameraList& cameraList) const
{
	if (m_pendingRequests.empty())
		return;

	const Request& request = m_pendingRequests.front();

	Wolf::AABB entityAABB = request.m_modelData->mesh->getAABB();
	float entityHeight = entityAABB.getMax().y - entityAABB.getMin().y;

	m_camera->setPosition(entityAABB.getCenter() + glm::vec3(-entityHeight, entityHeight, -entityHeight));
	m_camera->setPhi(-0.645398319f);
	m_camera->setTheta(glm::quarter_pi<float>());
	m_camera->setFar(1000'000.f);

	cameraList.addCameraForThisFrame(m_camera.get(), CommonCameraIndices::CAMERA_IDX_THUMBNAIL_GENERATION);
}

void ThumbnailsGenerationPass::addRequestBeforeFrame(const Request& request)
{
	m_pendingRequests.push_back(request);
	if (request.m_modelData->animationData)
	{
		uint32_t totalImagesToDraw = computeTotalImageToDraw(request);
		m_pendingRequests.back().m_imageLeftToDraw = totalImagesToDraw;
	}
	else
	{
		m_pendingRequests.back().m_imageLeftToDraw = 1;
	}
}
