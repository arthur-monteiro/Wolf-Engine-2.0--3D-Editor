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

#include "CommonLayouts.h"

ThumbnailsGenerationPass::ThumbnailsGenerationPass()
{
	m_camera.reset(new Wolf::FirstPersonCamera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f, 1.0f));
}

void ThumbnailsGenerationPass::initializeResources(const Wolf::InitializationContext& context)
{
	Wolf::CreateImageInfo createRenderTargetInfo;
	createRenderTargetInfo.extent = { OUTPUT_SIZE, OUTPUT_SIZE, 1 };
	createRenderTargetInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	createRenderTargetInfo.format = OUTPUT_FORMAT;
	createRenderTargetInfo.mipLevelCount = 1;
	m_renderTargetImage.reset(Wolf::Image::createImage(createRenderTargetInfo));
	Wolf::Attachment color = Wolf::Attachment({ OUTPUT_SIZE, OUTPUT_SIZE }, OUTPUT_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ATTACHMENT_STORE_OP_STORE,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, m_renderTargetImage->getDefaultImageView());

	Wolf::CreateImageInfo depthImageCreateInfo;
	depthImageCreateInfo.format = context.depthFormat;
	depthImageCreateInfo.extent.width = OUTPUT_SIZE;
	depthImageCreateInfo.extent.height = OUTPUT_SIZE;
	depthImageCreateInfo.extent.depth = 1;
	depthImageCreateInfo.mipLevelCount = 1;
	depthImageCreateInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	m_depthImage.reset(Wolf::Image::createImage(depthImageCreateInfo));
	Wolf::Attachment depth = Wolf::Attachment({ OUTPUT_SIZE, OUTPUT_SIZE }, context.depthFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_STORE,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, m_depthImage->getDefaultImageView());

	Wolf::CreateImageInfo createCopyInfo;
	createCopyInfo.extent = { OUTPUT_SIZE, OUTPUT_SIZE, 1 };
	createCopyInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
	m_vertexShaderParser.reset(new Wolf::ShaderParser("Shaders/thumbnailsGeneration/defaultShader.vert", {}, 0));
	m_fragmentShaderParser.reset(new Wolf::ShaderParser("Shaders/thumbnailsGeneration/defaultShader.frag", {}, -1, 1));

	pipelineCreateInfo.shaderCreateInfos.resize(2);
	m_vertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
	pipelineCreateInfo.shaderCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	m_fragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
	pipelineCreateInfo.shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

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

	m_pipeline.reset(Wolf::Pipeline::createRenderingPipeline(pipelineCreateInfo));
}

void ThumbnailsGenerationPass::resize(const Wolf::InitializationContext& context)
{
	// Nothing to do
}

void ThumbnailsGenerationPass::record(const Wolf::RecordContext& context)
{
	if (!m_currentRequests.empty())
	{
		Request& previousRequest = m_currentRequests.front();

		const void* outputBytes = m_copyImage->map();
		VkSubresourceLayout copyResourceLayout;
		m_copyImage->getResourceLayout(copyResourceLayout);
		stbi_write_png(previousRequest.outputFullFilepath.c_str(), OUTPUT_SIZE, OUTPUT_SIZE, 4, outputBytes, static_cast<int>(copyResourceLayout.rowPitch));
		m_copyImage->unmap();

		m_currentRequests.pop_front();
	}

	if (m_pendingRequests.empty())
	{
		m_drawRecordedThisFrame = false;
		return;
	}

	Request& request = m_pendingRequests.front();
	m_currentRequests.push_back(request);

	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "Thumbnails generation pass");

	std::vector<VkClearValue> clearValues(2);
	clearValues[0] = { 0.1f, 0.1f, 0.1f, 1.0f };
	clearValues[1] = { 1.0f };
	m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffer, clearValues);

	m_commandBuffer->bindPipeline(m_pipeline.get());
	m_commandBuffer->bindDescriptorSet(context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_THUMBNAIL_GENERATION)->getDescriptorSet(), 0, *m_pipeline);
	m_commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 1, *m_pipeline);
	request.modelData->mesh->draw(*m_commandBuffer, 0);

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

	copyRegion.extent = m_copyImage->getExtent();

	m_renderTargetImage->setImageLayoutWithoutOperation(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); // at this point, render pass should have set final layout
	m_copyImage->recordCopyGPUImage(*m_renderTargetImage, copyRegion, *m_commandBuffer);

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();

	m_drawRecordedThisFrame = true;
	m_pendingRequests.pop_front();
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

	Wolf::AABB entityAABB = request.modelData->mesh->getAABB();
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
}
