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

void AssetImageInterface::deleteImageData()
{
	m_mipData.clear();
	m_dataOnCPUStatus = DataOnCPUStatus::DELETED;
}

void AssetImageInterface::releaseImages()
{
	m_images.clear();
}

bool AssetImageInterface::generateThumbnail(const std::string& fullFilePath, const std::string& iconPath)
{
	if (m_images.empty())
	{
		Wolf::Debug::sendCriticalError("Cannot create thumbnail if not image is created");
	}
	Wolf::Image& image = *(m_images.begin()->second);
	Wolf::Extent3D imageExtent = image.getExtent();
	Wolf::Format imageFormat = image.getFormat();

	auto computeThumbnailData = [this, &imageExtent, &imageFormat](std::vector<Wolf::ImageCompression::RGBA8>& out, float samplingZ = 0.0f)
	{
		std::vector<Wolf::ImageCompression::RGBA8> RGBA8Pixels;
		std::vector<Wolf::ImageCompression::RG8> RG8Pixels;
		if (imageFormat == Wolf::Format::BC1_RGB_SRGB_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC1, m_mipData[0].data(), { imageExtent.width, imageExtent.height }, RGBA8Pixels);
		}
		else if (imageFormat == Wolf::Format::BC3_UNORM_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC3, m_mipData[0].data(), { imageExtent.width, imageExtent.height }, RGBA8Pixels);
		}
		else if (imageFormat == Wolf::Format::BC5_UNORM_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC5, m_mipData[0].data(), { imageExtent.width, imageExtent.height }, RG8Pixels);
		}

		bool hadAnErrorDuringGeneration = false;
		for (uint32_t x = 0; x < ThumbnailsGenerationPass::OUTPUT_SIZE; ++x)
		{
			for (uint32_t y = 0; y < ThumbnailsGenerationPass::OUTPUT_SIZE; ++y)
			{
				// We don't do bilinear here... Good enough?
				glm::vec3 samplingCoords = glm::vec3(static_cast<float>(x) / static_cast<float>(ThumbnailsGenerationPass::OUTPUT_SIZE), static_cast<float>(y) / static_cast<float>(ThumbnailsGenerationPass::OUTPUT_SIZE), samplingZ);
				glm::ivec3 samplingIndices = glm::ivec3(samplingCoords * glm::vec3(static_cast<float>(imageExtent.width), static_cast<float>(imageExtent.height), static_cast<float>(imageExtent.depth)));
				uint32_t samplingIndex = samplingIndices.x + samplingIndices.y * imageExtent.width + samplingIndices.z * imageExtent.width * imageExtent.height;

				Wolf::ImageCompression::RGBA8& pixel = out[x + y * ThumbnailsGenerationPass::OUTPUT_SIZE];

				if (imageFormat == Wolf::Format::R32G32B32A32_SFLOAT)
				{
					glm::vec4* pixels = reinterpret_cast<glm::vec4*>(m_mipData[0].data());
					pixel.r = glm::min(1.0f, pixels[samplingIndex].r) * 255.0f;
					pixel.g = glm::min(1.0f, pixels[samplingIndex].g) * 255.0f;
					pixel.b = glm::min(1.0f, pixels[samplingIndex].b) * 255.0f;
					pixel.a = 255.0f;
				}
				else if (imageFormat == Wolf::Format::R16G16B16A16_SFLOAT)
				{
					uint16_t* pixels = reinterpret_cast<uint16_t*>(m_mipData[0].data());
					pixel.r = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex])) * 255.0f;
					pixel.g = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex + 1])) * 255.0f;
					pixel.b = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex + 2])) * 255.0f;
					pixel.a = 255.0f;
				}
				else if (imageFormat == Wolf::Format::R8G8B8A8_UNORM)
				{
					uint8_t* pixels = m_mipData[0].data();
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

		for (uint32_t z = 0; z < imageExtent.depth; ++z)
		{
			std::vector<Wolf::ImageCompression::RGBA8> framePixels(ThumbnailsGenerationPass::OUTPUT_SIZE * ThumbnailsGenerationPass::OUTPUT_SIZE);
			if (!computeThumbnailData(framePixels, static_cast<float>(z) / static_cast<float>(imageExtent.depth)))
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