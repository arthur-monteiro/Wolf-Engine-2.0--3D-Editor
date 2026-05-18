#include "AssetImageInterface.h"

#include <glm/gtc/packing.hpp>
#include <stb_image_write.h>

#include <ImageCompression.h>

#include "EditorConfiguration.h"
#include "ImageFormatter.h"
#include "ThumbnailsGenerationPass.h"

AssetImageInterface::AssetImageInterface(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, bool needThumbnailsGeneration)
: m_editorPushDataToGPU(editorPushDataToGPU), m_thumbnailGenerationRequested(!g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration)
{
}

void AssetImageInterface::requestImageLoading(const LoadingRequest& loadingRequest)
{
	m_loadingRequestsMutex.lock();
	m_imageLoadingRequests.push(loadingRequest);
	m_loadingRequestsMutex.unlock();
}

Wolf::ResourceNonOwner<Wolf::Image> AssetImageInterface::getImage(Wolf::Format format)
{
	if (!m_images.contains(format))
	{
		Wolf::Debug::sendCriticalError("Image is not created for requested format");
	}
	return m_images[format].createNonOwnerResource();
}

const uint8_t* AssetImageInterface::getMipData(uint32_t mipLevel, Wolf::Format format) const
{
	if (!m_cpuData.contains(format))
	{
		Wolf::Debug::sendError("Data requested in not here");
		return nullptr;
	}

	return m_cpuData.at(format).m_mipData[mipLevel].data();
}

void AssetImageInterface::deleteImageData(Wolf::Format format)
{
	m_cpuData.erase(format);
}

void AssetImageInterface::releaseImages()
{
	m_images.clear();
}

bool AssetImageInterface::generateThumbnail(const std::string& fullFilePath, const std::string& iconPath)
{
	if (m_cpuData.empty())
	{
		Wolf::Debug::sendCriticalError("Cannot create thumbnail if not data stored on the CPU");
	}
	Wolf::Format imageFormat = m_cpuData.begin()->first;
	CPUData& data = m_cpuData.begin()->second;

	auto computeThumbnailData = [this, &imageFormat, &data](std::vector<Wolf::ImageCompression::RGBA8>& out, float samplingZ = 0.0f)
	{
		std::vector<Wolf::ImageCompression::RGBA8> RGBA8Pixels;
		std::vector<Wolf::ImageCompression::RG8> RG8Pixels;
		if (imageFormat == Wolf::Format::BC1_RGB_SRGB_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC1, data.m_mipData[0].data(), { data.m_extent.width, data.m_extent.height }, RGBA8Pixels);
		}
		else if (imageFormat == Wolf::Format::BC3_UNORM_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC3, data.m_mipData[0].data(), { data.m_extent.width, data.m_extent.height }, RGBA8Pixels);
		}
		else if (imageFormat == Wolf::Format::BC5_UNORM_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC5, data.m_mipData[0].data(), { data.m_extent.width, data.m_extent.height }, RG8Pixels);
		}

		bool hadAnErrorDuringGeneration = false;
		for (uint32_t x = 0; x < ThumbnailsGenerationPass::OUTPUT_SIZE; ++x)
		{
			for (uint32_t y = 0; y < ThumbnailsGenerationPass::OUTPUT_SIZE; ++y)
			{
				// We don't do bilinear here... Good enough?
				glm::vec3 samplingCoords = glm::vec3(static_cast<float>(x) / static_cast<float>(ThumbnailsGenerationPass::OUTPUT_SIZE), static_cast<float>(y) / static_cast<float>(ThumbnailsGenerationPass::OUTPUT_SIZE), samplingZ);
				glm::ivec3 samplingIndices = glm::ivec3(samplingCoords * glm::vec3(static_cast<float>(data.m_extent.width), static_cast<float>(data.m_extent.height), static_cast<float>(data.m_extent.depth)));
				uint32_t samplingIndex = samplingIndices.x + samplingIndices.y * data.m_extent.width + samplingIndices.z * data.m_extent.width * data.m_extent.height;

				Wolf::ImageCompression::RGBA8& pixel = out[x + y * ThumbnailsGenerationPass::OUTPUT_SIZE];

				if (imageFormat == Wolf::Format::R32G32B32A32_SFLOAT)
				{
					glm::vec4* pixels = reinterpret_cast<glm::vec4*>(data.m_mipData[0].data());
					pixel.r = glm::min(1.0f, pixels[samplingIndex].r) * 255.0f;
					pixel.g = glm::min(1.0f, pixels[samplingIndex].g) * 255.0f;
					pixel.b = glm::min(1.0f, pixels[samplingIndex].b) * 255.0f;
					pixel.a = 255.0f;
				}
				else if (imageFormat == Wolf::Format::R16G16B16A16_SFLOAT)
				{
					uint16_t* pixels = reinterpret_cast<uint16_t*>(data.m_mipData[0].data());
					pixel.r = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex])) * 255.0f;
					pixel.g = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex + 1])) * 255.0f;
					pixel.b = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex + 2])) * 255.0f;
					pixel.a = 255.0f;
				}
				else if (imageFormat == Wolf::Format::R8G8B8A8_UNORM)
				{
					uint8_t* pixels = data.m_mipData[0].data();
					pixel.r = pixels[4 * samplingIndex];
					pixel.g = pixels[4 * samplingIndex + 1];
					pixel.b = pixels[4 * samplingIndex + 2];
					pixel.a = pixels[4 * samplingIndex + 3];
				}
				else if (imageFormat == Wolf::Format::BC1_RGB_SRGB_BLOCK || imageFormat == Wolf::Format::BC3_UNORM_BLOCK)
				{
					pixel = RGBA8Pixels[samplingIndex];
					pixel.a = 255;
				}
				else if (imageFormat == Wolf::Format::BC5_UNORM_BLOCK)
				{
					pixel.r = RG8Pixels[samplingIndex].r;
					pixel.g = RG8Pixels[samplingIndex].g;
					pixel.b = 0;
					pixel.a = 255;
				}
				else
				{
					Wolf::Debug::sendMessageOnce("Image format is not supported for thumbnail generation", Wolf::Debug::Severity::WARNING, this);
					hadAnErrorDuringGeneration = true;
				}
			}
		}

		return !hadAnErrorDuringGeneration;
	};

	std::string extension = fullFilePath.substr(fullFilePath.find_last_of('.') + 1);
	bool hadAnErrorDuringGeneration = false;

	if (extension == "cube")
	{
		GifEncoder gifEncoder;

		static constexpr int quality = 30;
		static constexpr bool useGlobalColorMap = false;
		static constexpr int loop = 0;
		static constexpr int preAllocSize = ThumbnailsGenerationPass::OUTPUT_SIZE * ThumbnailsGenerationPass::OUTPUT_SIZE * 3;

		if (!gifEncoder.open(iconPath, ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE, quality, useGlobalColorMap, loop, preAllocSize))
		{
			Wolf::Debug::sendError("Error when opening gif file");
		}

		for (uint32_t z = 0; z < data.m_extent.depth; ++z)
		{
			std::vector<Wolf::ImageCompression::RGBA8> framePixels(ThumbnailsGenerationPass::OUTPUT_SIZE * ThumbnailsGenerationPass::OUTPUT_SIZE);
			if (!computeThumbnailData(framePixels, static_cast<float>(z) / static_cast<float>(data.m_extent.depth)))
			{
				Wolf::Debug::sendMessageOnce("Error computing frame data for " + fullFilePath + " thumbnail generation", Wolf::Debug::Severity::ERROR, this);
				hadAnErrorDuringGeneration = true;
			}

			gifEncoder.push(GifEncoder::PIXEL_FORMAT_RGBA, reinterpret_cast<const uint8_t*>(framePixels.data()), ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE,
				static_cast<uint32_t>(ThumbnailsGenerationPass::DELAY_BETWEEN_ICON_FRAMES_MS) / 10);
		}

		gifEncoder.close();
	}
	else
	{
		std::vector<Wolf::ImageCompression::RGBA8> thumbnailPixels(ThumbnailsGenerationPass::OUTPUT_SIZE * ThumbnailsGenerationPass::OUTPUT_SIZE);

		if (computeThumbnailData(thumbnailPixels))
		{
			stbi_write_png(iconPath.c_str(), ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE,
				4, thumbnailPixels.data(), ThumbnailsGenerationPass::OUTPUT_SIZE * sizeof(Wolf::ImageCompression::RGBA8));
		}
		else
		{
			hadAnErrorDuringGeneration = true;
		}
	}

	return !hadAnErrorDuringGeneration;
}

AssetImageInterface::KeepDataMode AssetImageInterface::computeNeededKeepDataMode(Wolf::Format format, KeepDataMode requestedKeepDataMode) const
{
	bool keepGPUData = false;
	if (requestedKeepDataMode == KeepDataMode::ONLY_GPU || requestedKeepDataMode == KeepDataMode::CPU_AND_GPU)
	{
		keepGPUData = !m_images.contains(format);
	}

	bool keepCPUData = false;
	if (requestedKeepDataMode == KeepDataMode::ONLY_CPU || requestedKeepDataMode == KeepDataMode::CPU_AND_GPU)
	{
		keepCPUData = !m_cpuData.contains(format);
	}

	if (keepGPUData && keepCPUData)
		return KeepDataMode::CPU_AND_GPU;
	else if (keepGPUData)
		return KeepDataMode::ONLY_GPU;
	else if (keepCPUData)
		return KeepDataMode::ONLY_CPU;
	else
		return KeepDataMode::DONT_KEEP;
}
