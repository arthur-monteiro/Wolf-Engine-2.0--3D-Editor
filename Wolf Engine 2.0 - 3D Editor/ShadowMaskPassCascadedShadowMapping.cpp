#include "ShadowMaskPassCascadedShadowMapping.h"

#include <fstream>
#include <random>

#include <ProfilerCommon.h>

#include "CommonLayouts.h"
#include "DebugMarker.h"
#include "DescriptorSetGenerator.h"
#include "LightManager.h"
#include "PreDepthPass.h"

ShadowMaskPassCascadedShadowMapping::ShadowMaskPassCascadedShadowMapping(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass,	const Wolf::ResourceNonOwner<CascadedShadowMapsPass>& csmPass)
		: m_editorParams(editorParams), m_preDepthPass(preDepthPass), m_csmPass(csmPass)
{

}

void ShadowMaskPassCascadedShadowMapping::initializeResources(const Wolf::InitializationContext& context)
{
	m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::COMPUTE, false /* isTransient */));
	m_semaphore.reset(Wolf::Semaphore::createSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));

	Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
	addShaderCode(shaderCodeToAdd, 0);
	m_computeShaderParser.reset(new Wolf::ShaderParser("Shaders/cascadedShadowMapping/shader.comp", {}, 2, -1, -1, 
		Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));

	// Resource to compute shadow
	m_outputComputeDescriptorSetLayoutGenerator.addImages(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE | Wolf::ShaderStageFlagBits::FRAGMENT, 0, 1); // input depth
	m_outputComputeDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::COMPUTE | Wolf::ShaderStageFlagBits::FRAGMENT,                            1);
	m_outputComputeDescriptorSetLayoutGenerator.addImages(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, Wolf::ShaderStageFlagBits::COMPUTE | Wolf::ShaderStageFlagBits::FRAGMENT, 2, CascadedShadowMapsPass::CASCADE_COUNT); // cascade depth images
	m_outputComputeDescriptorSetLayoutGenerator.addSampler(Wolf::ShaderStageFlagBits::COMPUTE | Wolf::ShaderStageFlagBits::FRAGMENT,                                  3);
	m_outputComputeDescriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::COMPUTE | Wolf::ShaderStageFlagBits::FRAGMENT,                     4); // noise map
	m_outputComputeDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_outputComputeDescriptorSetLayoutGenerator.getDescriptorLayouts()));

	// Resource only for this pass
	m_descriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::COMPUTE, 0); // output mask
	m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

	m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(ShadowUBData)));
	m_shadowMapsSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1.0f, VK_FILTER_LINEAR));

	// Noise
	Wolf::CreateImageInfo noiseImageCreateInfo;
	noiseImageCreateInfo.extent = { NOISE_TEXTURE_SIZE_PER_SIDE, NOISE_TEXTURE_SIZE_PER_SIDE, NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE };
	noiseImageCreateInfo.format = Wolf::Format::R32G32_SFLOAT;
	noiseImageCreateInfo.mipLevelCount = 1;
	noiseImageCreateInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
	m_noiseImage.reset(Wolf::Image::createImage(noiseImageCreateInfo));

	std::vector<float> noiseData(static_cast<size_t>(NOISE_TEXTURE_SIZE_PER_SIDE) * NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE * 2u);
	for (uint32_t texY = 0; texY < NOISE_TEXTURE_SIZE_PER_SIDE; ++texY)
	{
		for (uint32_t texX = 0; texX < NOISE_TEXTURE_SIZE_PER_SIDE; ++texX)
		{
			for (uint32_t v = 0; v < 4; ++v)
			{
				for (uint32_t u = 0; u < 4; u++)
				{
					float x = (static_cast<float>(u) + 0.5f + jitter()) / static_cast<float>(NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE);
					float y = (static_cast<float>(v) + 0.5f + jitter()) / static_cast<float>(NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE);

					const uint32_t patternIdx = u + v * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE;
					const uint32_t idx = texX + texY * NOISE_TEXTURE_SIZE_PER_SIDE + patternIdx * (NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_SIZE_PER_SIDE);

					constexpr float PI = 3.14159265358979323846264338327950288f;
					noiseData[2 * idx] = sqrtf(y) * cosf(2.0f * PI * x);
					noiseData[2 * idx + 1] = sqrtf(y) * sinf(2.0f * PI * x);
				}
			}
		}
	}
	m_noiseImage->copyCPUBuffer(reinterpret_cast<unsigned char*>(noiseData.data()), Wolf::Image::SampledInFragmentShader());

	m_noiseSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1.0f, VK_FILTER_NEAREST));

	createPipeline();
	createOutputImages(context.swapChainWidth, context.swapChainHeight);

	m_outputComputeDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_outputComputeDescriptorSetLayout));
	m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));

	m_outputMaskDescriptorSetLayoutGenerator.addStorageImage(Wolf::ShaderStageFlagBits::FRAGMENT, 0);
	m_outputMaskDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_outputMaskDescriptorSetLayoutGenerator.getDescriptorLayouts()));

	m_outputMaskDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_outputMaskDescriptorSetLayout));

	updateDescriptorSet();

	for (float& noiseRotation : m_noiseRotations)
	{
		noiseRotation = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 3.14f * 2.0f;
	}
}

void ShadowMaskPassCascadedShadowMapping::resize(const Wolf::InitializationContext& context)
{
	createOutputImages(context.swapChainWidth, context.swapChainHeight);
	updateDescriptorSet();
}

void ShadowMaskPassCascadedShadowMapping::record(const Wolf::RecordContext& context)
{
	PROFILE_FUNCTION

	uint32_t sunLightCount = context.lightManager->getSunLightCount();
	if (sunLightCount == 0)
	{
		return;
	}

	const Wolf::CameraInterface* camera = context.cameraList->getCamera(CommonCameraIndices::CAMERA_IDX_MAIN);

	/* Update data */
	const Wolf::Viewport renderViewport = m_editorParams->getRenderViewport();

	ShadowUBData shadowUBData;
	shadowUBData.cascadeSplits = glm::vec4(m_csmPass->getCascadeSplit(0), m_csmPass->getCascadeSplit(1), m_csmPass->getCascadeSplit(2), m_csmPass->getCascadeSplit(3));
	for (uint32_t cascadeIdx = 0; cascadeIdx < CascadedShadowMapsPass::CASCADE_COUNT; ++cascadeIdx)
	{
		m_csmPass->getCascadeMatrix(cascadeIdx, shadowUBData.cascadeMatrices[cascadeIdx]);
	}

	const glm::vec2 referenceScale = glm::vec2(glm::length(shadowUBData.cascadeMatrices[0][0]), glm::length(shadowUBData.cascadeMatrices[0][1]));
	for (uint32_t cascadeIdx = 0; cascadeIdx < CascadedShadowMapsPass::CASCADE_COUNT / 2; ++cascadeIdx)
	{
		glm::vec2 cascadeScale1 = glm::vec2(glm::length(shadowUBData.cascadeMatrices[2 * cascadeIdx][0]), glm::length(shadowUBData.cascadeMatrices[2 * cascadeIdx][1]));
		glm::vec2 cascadeScale2 = glm::vec2(glm::length(shadowUBData.cascadeMatrices[2 * cascadeIdx + 1][0]), glm::length(shadowUBData.cascadeMatrices[2 * cascadeIdx + 1][1]));
		shadowUBData.cascadeScales[cascadeIdx] = glm::vec4(cascadeScale1 / referenceScale, cascadeScale2 / referenceScale);
	}

	shadowUBData.cascadeTextureSize = glm::uvec4(m_csmPass->getCascadeTextureSize(0), m_csmPass->getCascadeTextureSize(1), m_csmPass->getCascadeTextureSize(2), m_csmPass->getCascadeTextureSize(3));

	shadowUBData.noiseRotation = m_noiseRotations[context.currentFrameIdx % m_noiseRotations.size()];

	shadowUBData.viewportOffset = glm::uvec2(m_editorParams->getRenderViewport().x, m_editorParams->getRenderViewport().y);
	shadowUBData.viewportSize = glm::uvec2(m_editorParams->getRenderViewport().width, m_editorParams->getRenderViewport().height);

	m_uniformBuffer->transferCPUMemory((void*)&shadowUBData, sizeof(shadowUBData), 0);

	/* Command buffer record */
	m_commandBuffer->beginCommandBuffer();

	Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::computePassDebugColor, "CSM Shadow Mask Compute Pass");

	m_commandBuffer->bindDescriptorSet(m_outputComputeDescriptorSet.createConstNonOwnerResource(), 0, *m_pipeline);
	m_commandBuffer->bindDescriptorSet(m_descriptorSet.get(), 1, *m_pipeline);
	m_commandBuffer->bindDescriptorSet(camera->getDescriptorSet(), 2, *m_pipeline);
	m_commandBuffer->bindPipeline(m_pipeline.get());

	constexpr Wolf::Extent3D dispatchGroups = { 16, 16, 1 };
	const uint32_t groupSizeX = m_outputMask->getExtent().width % dispatchGroups.width != 0 ? m_outputMask->getExtent().width / dispatchGroups.width + 1 : m_outputMask->getExtent().width / dispatchGroups.width;
	const uint32_t groupSizeY = m_outputMask->getExtent().height % dispatchGroups.height != 0 ? m_outputMask->getExtent().height / dispatchGroups.height + 1 : m_outputMask->getExtent().height / dispatchGroups.height;
	m_commandBuffer->dispatch(groupSizeX, groupSizeY, dispatchGroups.depth);

	Wolf::DebugMarker::endRegion(m_commandBuffer.get());

	m_commandBuffer->endCommandBuffer();
}

void ShadowMaskPassCascadedShadowMapping::submit(const Wolf::SubmitContext& context)
{
	if (!m_csmPass->wasEnabledThisFrame())
	{
		return;
	}

	const std::vector waitSemaphores{ m_csmPass->getSemaphore(), m_preDepthPass->getSemaphore() };
	const std::vector<const Wolf::Semaphore*> signalSemaphores{ m_semaphore.get() };
	m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);

	if (m_computeShaderParser->compileIfFileHasBeenModified())
	{
		context.graphicAPIManager->waitIdle();
		createPipeline();
	}
}

void ShadowMaskPassCascadedShadowMapping::addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const
{
	std::ifstream inFile("Shaders/cascadedShadowMapping/computeShadows.glsl");
	std::string line;
	while (std::getline(inFile, line))
	{
		const std::string& descriptorSlotToken = "£CSM_DESCRIPTOR_SLOT";
		size_t descriptorSlotTokenPos = line.find(descriptorSlotToken);
		while (descriptorSlotTokenPos != std::string::npos)
		{
			line.replace(descriptorSlotTokenPos, descriptorSlotToken.length(), std::to_string(bindingSlot));
			descriptorSlotTokenPos = line.find(descriptorSlotToken);
		}
		inOutShaderCodeToAdd.codeString += line + '\n';
	}
}

void ShadowMaskPassCascadedShadowMapping::createOutputImages(uint32_t width, uint32_t height)
{
	Wolf::CreateImageInfo createImageInfo;
	createImageInfo.extent = { width, height, 1 };
	createImageInfo.format = Wolf::Format::R32G32_SFLOAT;
	createImageInfo.mipLevelCount = 1;
	createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	createImageInfo.usage = Wolf::ImageUsageFlagBits::STORAGE | Wolf::ImageUsageFlagBits::SAMPLED;
	m_outputMask.reset(Wolf::Image::createImage(createImageInfo));
	m_outputMask->setImageLayout({ VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });
}

void ShadowMaskPassCascadedShadowMapping::createPipeline()
{
	// Compute shader parser
	std::vector<char> computeShaderCode;
	m_computeShaderParser->readCompiledShader(computeShaderCode);

	Wolf::ShaderCreateInfo computeShaderCreateInfo;
	computeShaderCreateInfo.shaderCode = computeShaderCode;
	computeShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;

	std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts;
	descriptorSetLayouts.reserve(3);
	descriptorSetLayouts.emplace_back(m_outputComputeDescriptorSetLayout.createConstNonOwnerResource());
	descriptorSetLayouts.emplace_back(m_descriptorSetLayout.createConstNonOwnerResource());
	descriptorSetLayouts.emplace_back(Wolf::GraphicCameraInterface::getDescriptorSetLayout().createConstNonOwnerResource());

	m_pipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderCreateInfo, descriptorSetLayouts));
}

void ShadowMaskPassCascadedShadowMapping::updateDescriptorSet() const
{
	Wolf::DescriptorSetGenerator outputDescriptorSetGenerator(m_outputComputeDescriptorSetLayoutGenerator.getDescriptorLayouts());

	Wolf::DescriptorSetGenerator::ImageDescription preDepthImageDesc;
	preDepthImageDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	preDepthImageDesc.imageView = m_preDepthPass->getOutput()->getDefaultImageView();
	outputDescriptorSetGenerator.setImage(0, preDepthImageDesc);
	outputDescriptorSetGenerator.setUniformBuffer(1, *m_uniformBuffer);
	std::vector<Wolf::DescriptorSetGenerator::ImageDescription> shadowMapImageDescriptions(CascadedShadowMapsPass::CASCADE_COUNT);
	for (uint32_t i = 0; i < CascadedShadowMapsPass::CASCADE_COUNT; ++i)
	{
		shadowMapImageDescriptions[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		shadowMapImageDescriptions[i].imageView = m_csmPass->getShadowMap(i)->getDefaultImageView();
	}
	outputDescriptorSetGenerator.setImages(2, shadowMapImageDescriptions);
	outputDescriptorSetGenerator.setSampler(3, *m_shadowMapsSampler);
	outputDescriptorSetGenerator.setCombinedImageSampler(4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_noiseImage->getDefaultImageView(), *m_noiseSampler);

	m_outputComputeDescriptorSet->update(outputDescriptorSetGenerator.getDescriptorSetCreateInfo());

	// Only for this pass
	{
		Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
		Wolf::DescriptorSetGenerator::ImageDescription outputImageDesc;
		outputImageDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		outputImageDesc.imageView = m_outputMask->getDefaultImageView();
		descriptorSetGenerator.setImage(0, outputImageDesc);

		m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
	}

	{
		Wolf::DescriptorSetGenerator descriptorSetGenerator(m_outputMaskDescriptorSetLayoutGenerator.getDescriptorLayouts());
		Wolf::DescriptorSetGenerator::ImageDescription shadowMaskDesc;
		shadowMaskDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		shadowMaskDesc.imageView = m_outputMask->getDefaultImageView();
		descriptorSetGenerator.setImage(0, shadowMaskDesc);

		m_outputMaskDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
	}
}

float ShadowMaskPassCascadedShadowMapping::jitter()
{
	static std::default_random_engine generator;
	static std::uniform_real_distribution distrib(-0.5f, 0.5f);
	return distrib(generator);
}

