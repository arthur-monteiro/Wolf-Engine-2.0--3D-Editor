#include "TextureSetLoader.h"

#include <filesystem>
#include <fstream>

#include <Configuration.h>

#include "AssetManager.h"
#include "CodeFileHashes.h"
#include "ConfigurationHelper.h"
#include "EditorConfiguration.h"
#include "ImageFileLoader.h"
#include "MipMapGenerator.h"
#include "PushDataToGPU.h"
#include "Timer.h"
#include "VirtualTextureManager.h"

TextureSetLoader::TextureSetLoader(const TextureSetFileInfoGGX& textureSet, const OutputLayout& outputLayout, bool useCache, const Wolf::ResourceReference<AssetManager>& assetManager) : m_useCache(useCache)
{
	Wolf::Debug::sendInfo("Loading material " + textureSet.name);
	Wolf::Timer globalTimer("Loading material " + textureSet.name);

	// Albedo
	if (!textureSet.albedo.empty())
	{
		Wolf::Debug::sendInfo("Loading albedo " + textureSet.albedo);
		Wolf::Timer albedoTimer("Loading albedo " + textureSet.albedo);

		if (outputLayout.albedoCompression != DEFAULT_ALBEDO_COMPRESSION)
		{
			Wolf::Debug::sendCriticalError("Albedo compression must be BC1");
		}

		m_imageAssetIds[0] = assetManager->addImage(textureSet.albedo, true, Wolf::Format::BC1_RGB_SRGB_BLOCK, false, true, true);
	}
	else
	{
		m_imageAssetIds[0] = NO_ASSET;
	}

	// Normal
	if (!textureSet.normal.empty())
	{
		Wolf::Debug::sendInfo("Loading normal " + textureSet.normal);
		Wolf::Timer albedoTimer("Loading normal " + textureSet.normal);

		if (outputLayout.normalCompression != DEFAULT_NORMAL_COMPRESSION)
		{
			Wolf::Debug::sendError("Normal compression must be BC5");
		}

		m_imageAssetIds[1] = assetManager->addImage(textureSet.normal, true, Wolf::Format::BC5_UNORM_BLOCK, false, true, true);
	}
	else
	{
		m_imageAssetIds[1] = NO_ASSET;
	}

	if (!textureSet.roughness.empty())
	{
		std::string message = "Loading combined image " + textureSet.roughness + ", " + textureSet.metalness + ", " + textureSet.ao + ", " + textureSet.anisoStrength;
		Wolf::Debug::sendInfo(message);
		Wolf::Timer albedoTimer(message);

		m_imageAssetIds[2] = assetManager->addCombinedImage(textureSet.roughness, textureSet.metalness, textureSet.ao, textureSet.anisoStrength, true,
			Wolf::Format::BC3_UNORM_BLOCK, false, true, true);
	}
	else
	{
		m_imageAssetIds[2] = NO_ASSET;
	}
}

TextureSetLoader::TextureSetLoader(const TextureSetFileInfoSixWayLighting& textureSet, const Wolf::ResourceReference<AssetManager>& assetManager)
{
	if (!textureSet.tex0.empty())
	{
		m_imageAssetIds[0] = assetManager->addImage(textureSet.tex0, true, Wolf::Format::R8G8B8A8_UNORM, false, true, true);
	}

	if (!textureSet.tex1.empty())
	{
		m_imageAssetIds[1] = assetManager->addImage(textureSet.tex1, true, Wolf::Format::R8G8B8A8_UNORM, false, true, true);
	}
}

TextureSetLoader::TextureSetLoader(const TextureSetFileInfoAlphaOnly& textureSet, const Wolf::ResourceReference<AssetManager>& assetManager)
{
	if (!textureSet.alphaMap.empty())
	{
		m_imageAssetIds[0] = assetManager->addImage(textureSet.alphaMap, true, Wolf::Format::R8G8B8A8_UNORM, false, true, true);
	}
}

void TextureSetLoader::transferImageTo(uint32_t idx, Wolf::ResourceUniqueOwner<Wolf::Image>& output)
{
	if (idx == 0 || idx == 1 || idx == 2)
		__debugbreak();

	return output.transferFrom(m_outputImages[idx]);
}

void TextureSetLoader::loadImageFile(const std::string& filename, Wolf::Format format, std::vector<Wolf::ImageCompression::RGBA8>& pixels, std::vector<std::vector<Wolf::ImageCompression::RGBA8>>& mipLevels,
	Wolf::Extent3D& outExtent) const
{
	const Wolf::ImageFileLoader imageFileLoader(filename);

	if (imageFileLoader.getCompression() == Wolf::ImageCompression::Compression::BC5)
	{
#ifndef __ANDROID__
		__debugbreak(); // deadcode?
#endif

		std::vector<Wolf::ImageCompression::RG8> RG8Pixels;
		Wolf::ImageCompression::uncompressImage(imageFileLoader.getCompression(), imageFileLoader.getPixels(), { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()) }, RG8Pixels);

		pixels.resize(RG8Pixels.size());
		auto computeZ = [](const Wolf::ImageCompression::RG8& pixel)
			{
				auto u8ToFloat = [](uint8_t value) { return 2.0f * static_cast<float>(value) / 255.0f - 1.0f; };
				return static_cast<uint8_t>(((glm::sqrt(1.0f - u8ToFloat(pixel.r) * u8ToFloat(pixel.r) - u8ToFloat(pixel.g) * u8ToFloat(pixel.g)) + 1.0f) * 0.5f) * 255.0f);
			};
		for (uint32_t i = 0; i < pixels.size(); ++i)
		{
			pixels[i] = Wolf::ImageCompression::RGBA8(RG8Pixels[i].r, RG8Pixels[i].g, computeZ(RG8Pixels[i]), 255);
		}
	}
	else if (imageFileLoader.getCompression() != Wolf::ImageCompression::Compression::NO_COMPRESSION)
	{
		Wolf::ImageCompression::uncompressImage(imageFileLoader.getCompression(), imageFileLoader.getPixels(), { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()) }, pixels);
	}
	else
	{
		pixels.assign(reinterpret_cast<Wolf::ImageCompression::RGBA8*>(imageFileLoader.getPixels()),
			reinterpret_cast<Wolf::ImageCompression::RGBA8*>(imageFileLoader.getPixels()) + static_cast<size_t>(imageFileLoader.getWidth()) * imageFileLoader.getHeight());
	}

	const Wolf::MipMapGenerator mipmapGenerator(reinterpret_cast<const unsigned char*>(pixels.data()), { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()) }, format);

	mipLevels.resize(mipmapGenerator.getMipLevelCount() - 1);
	for (uint32_t mipLevel = 1; mipLevel < mipmapGenerator.getMipLevelCount(); ++mipLevel)
	{
		const std::vector<unsigned char>& mipPixels = mipmapGenerator.getMipLevel(mipLevel);
		const Wolf::ImageCompression::RGBA8* mipPixelsAsColor = reinterpret_cast<const Wolf::ImageCompression::RGBA8*>(mipPixels.data());
		mipLevels[mipLevel - 1] = std::vector<Wolf::ImageCompression::RGBA8>(mipPixelsAsColor, mipPixelsAsColor + mipPixels.size() / sizeof(Wolf::ImageCompression::RGBA8));
	}

	outExtent = { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()), 1 };
}

void TextureSetLoader::loadImageFile(const std::string& filename, Wolf::Format format, std::vector<Wolf::ImageCompression::RG32F>& pixels, std::vector<std::vector<Wolf::ImageCompression::RG32F>>& mipLevels,
	Wolf::Extent3D& outExtent) const
{
	const Wolf::ImageFileLoader imageFileLoader(filename, false);

	if (imageFileLoader.getCompression() == Wolf::ImageCompression::Compression::BC5)
	{
		Wolf::Debug::sendCriticalError("Unhandled");
	}
	else if (imageFileLoader.getCompression() != Wolf::ImageCompression::Compression::NO_COMPRESSION)
	{
		Wolf::Debug::sendCriticalError("Unhandled");
	}
	else
	{
		// For some reason, loading a texture as floats (stbi_loadf) implies that texture should be HDR, which is not true for normals...

		//ImageCompression::RGBA32F* fullPixels = reinterpret_cast<ImageCompression::RGBA32F*>(imageFileLoader.getPixels());
		Wolf::ImageCompression::RGBA8* fullPixels = reinterpret_cast<Wolf::ImageCompression::RGBA8*>(imageFileLoader.getPixels());
		pixels.resize(static_cast<size_t>(imageFileLoader.getWidth()) * imageFileLoader.getHeight());

		for (uint32_t i = 0; i < pixels.size(); ++i)
		{
			//ImageCompression::RGBA32F fullPixel = fullPixels[i];
			Wolf::ImageCompression::RGBA8 fullPixel = fullPixels[i];
			glm::vec3 fullPixelAsVec = 2.0f * glm::vec3(static_cast<float>(fullPixel.r) / 255.0f, static_cast<float>(fullPixel.g) / 255.0f, static_cast<float>(fullPixel.b) / 255.0f) - glm::vec3(1.0f);
			fullPixelAsVec = glm::normalize(fullPixelAsVec);

			pixels[i] = Wolf::ImageCompression::RG32F(fullPixelAsVec.x, fullPixelAsVec.y);
		}
	}

	const Wolf::MipMapGenerator mipmapGenerator(reinterpret_cast<const unsigned char*>(pixels.data()), { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()) }, format);

	mipLevels.resize(mipmapGenerator.getMipLevelCount() - 1);
	for (uint32_t mipLevel = 1; mipLevel < mipmapGenerator.getMipLevelCount(); ++mipLevel)
	{
		const std::vector<unsigned char>& mipPixels = mipmapGenerator.getMipLevel(mipLevel);
		const Wolf::ImageCompression::RG32F* mipPixelsAsColor = reinterpret_cast<const Wolf::ImageCompression::RG32F*>(mipPixels.data());
		mipLevels[mipLevel - 1] = std::vector<Wolf::ImageCompression::RG32F>(mipPixelsAsColor, mipPixelsAsColor + mipPixels.size() / sizeof(Wolf::ImageCompression::RG32F));
	}

	outExtent = { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()), 1 };
}

Wolf::Format TextureSetLoader::findFormatFromCompression(Wolf::ImageCompression::Compression compression, bool sRGB)
{
	if (compression == Wolf::ImageCompression::Compression::BC1 || compression == Wolf::ImageCompression::Compression::BC3)
		return sRGB ? Wolf::Format::R8G8B8A8_SRGB : Wolf::Format::R8G8B8A8_UNORM;
	else if (compression == Wolf::ImageCompression::Compression::BC5)
		return Wolf::Format::R32G32_SFLOAT;
	else
	{
		Wolf::Debug::sendCriticalError("Unhandled case");
		return Wolf::Format::UNDEFINED;
	}
}

bool TextureSetLoader::createImageFileFromCache(const std::string& filename, bool sRGB, Wolf::ImageCompression::Compression compression, uint32_t imageIdx)
{
	std::string binFilename = filename + ".bin";
	if (std::filesystem::exists(binFilename))
	{
		std::ifstream input(binFilename, std::ios::in | std::ios::binary);

		uint64_t hash;
		input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
		if (hash != Wolf::HASH_TEXTURE_SET_LOADER_CPP)
		{
			Wolf::Debug::sendInfo("Cache found but hash is incorrect");
			return false;
		}

		Wolf::Extent3D extent;
		input.read(reinterpret_cast<char*>(&extent), sizeof(extent));

		uint32_t dataBytesCount;
		input.read(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));

		std::vector<uint8_t> data(dataBytesCount);
		input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

		uint32_t mipsCount;
		input.read(reinterpret_cast<char*>(&mipsCount), sizeof(mipsCount));

		std::vector<std::vector<uint8_t>> mipsData(mipsCount);
		std::vector<const uint8_t*> mipsDataPtr(mipsData.size());
		for (uint32_t i = 0; i < mipsData.size(); ++i)
		{
			input.read(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));
			mipsData[i].resize(dataBytesCount);
			input.read(reinterpret_cast<char*>(mipsData[i].data()), static_cast<std::streamsize>(mipsData[i].size()));

			mipsDataPtr[i] = mipsData[i].data();
		}

		Wolf::Format compressedFormat = Wolf::Format::UNDEFINED;
		if (compression == Wolf::ImageCompression::Compression::BC1)
			compressedFormat = sRGB ? Wolf::Format::BC1_RGB_SRGB_BLOCK : Wolf::Format::BC1_RGBA_UNORM_BLOCK;
		else if (compression == Wolf::ImageCompression::Compression::BC3)
			compressedFormat = sRGB ? Wolf::Format::BC3_SRGB_BLOCK : Wolf::Format::BC3_UNORM_BLOCK;
		else if (compression == Wolf::ImageCompression::Compression::BC5)
			compressedFormat = Wolf::Format::BC5_UNORM_BLOCK;
		else if (compression == Wolf::ImageCompression::Compression::NO_COMPRESSION)
			compressedFormat = sRGB ? Wolf::Format::R8G8B8A8_SRGB : Wolf::Format::R8G8B8A8_UNORM;

		createImageFromData(extent, compressedFormat, data.data(), mipsDataPtr, imageIdx);

		return true;
	}
	else if (filename[filename.size() - 4] == '.' && filename[filename.size() - 3] == 'd' && filename[filename.size() - 2] == 'd' && filename[filename.size() - 1] == 's')
	{
		const Wolf::ImageFileLoader imageFileLoader(filename, false);

		Wolf::Extent3D extent = { imageFileLoader.getWidth(), imageFileLoader.getHeight(), 1 };
		Wolf::Format format = imageFileLoader.getFormat();

		const std::vector<std::vector<uint8_t>> mipPixels = imageFileLoader.getMipPixels();
		std::vector<const unsigned char*> mipPtrs(mipPixels.size());
		for (uint32_t i = 0; i < mipPixels.size(); ++i)
		{
			mipPtrs[i] = mipPixels[i].data();
		}
		createImageFromData(extent, format, imageFileLoader.getPixels(), mipPtrs, imageIdx);

		return true;
	}

	Wolf::Debug::sendInfo("Cache not found");
	return false;
}

template <typename PixelType>
void TextureSetLoader::createImageFileFromSource(const std::string& filename, bool sRGB, Wolf::ImageCompression::Compression compression, uint32_t imageIdx)
{
	Wolf::Format format = findFormatFromCompression(compression, sRGB);

	std::vector<PixelType> pixels;
	std::vector<std::vector<PixelType>> mipLevels;
	Wolf::Extent3D extent;
	loadImageFile(filename, format, pixels, mipLevels, extent);

	if (pixels.size() < 4)
	{
		Wolf::Debug::sendWarning("Image size must be at least 4x4");
		pixels.resize(4 * 4);
		for (uint32_t i = 1; i < 4 * 4; ++i)
		{
			pixels[i] = pixels[0];
		}
		extent.width = extent.height = 4;
	}

	Wolf::Debug::sendInfo("Creating new cache");

	std::fstream outCacheFile(filename + ".bin", std::ios::out | std::ios::binary);

	/* Hash */
	uint64_t hash = Wolf::HASH_TEXTURE_SET_LOADER_CPP;
	outCacheFile.write(reinterpret_cast<char*>(&hash), sizeof(hash));
	outCacheFile.write(reinterpret_cast<char*>(&extent), sizeof(extent));

	if (compression == Wolf::ImageCompression::Compression::BC1)
		compressAndCreateImage<Wolf::ImageCompression::BC1>(mipLevels, pixels, extent, Wolf::Format::BC1_RGB_SRGB_BLOCK, filename, outCacheFile, imageIdx);
	else if (compression == Wolf::ImageCompression::Compression::BC3)
		compressAndCreateImage<Wolf::ImageCompression::BC3>(mipLevels, pixels, extent, Wolf::Format::BC3_SRGB_BLOCK, filename, outCacheFile, imageIdx);
	else if (compression == Wolf::ImageCompression::Compression::BC5)
		compressAndCreateImage<Wolf::ImageCompression::BC5>(mipLevels, pixels, extent, Wolf::Format::BC5_UNORM_BLOCK, filename, outCacheFile, imageIdx);
	else
		Wolf::Debug::sendError("Requested compression is not available");

	outCacheFile.close();
}

template <typename PixelType, typename CompressionType>
void TextureSetLoader::createSlicedCacheFromData(const std::string& binFolder, Wolf::Extent3D extent, const std::vector<PixelType>& pixels, const std::vector<std::vector<PixelType>>& mipLevels)
{
	if (extent.depth != 1 ||
		(extent.width > Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE && (extent.width % Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE) != 0) ||
		(extent.height > Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE && (extent.height % Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE) != 0))
	{
		Wolf::Debug::sendCriticalError("Wrong texture extent for virtual texture slicing: width/height must be a multiple of VIRTUAL_PAGE_SIZE when larger than a page, and depth must be 1");
	}

	Wolf::ConfigurationHelper::writeInfoToFile(binFolder + "info.txt", "width", extent.width);
	Wolf::ConfigurationHelper::writeInfoToFile(binFolder + "info.txt", "height", extent.height);

	Wolf::Extent3D maxSliceExtent{ Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE, Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE, 1 };

	for (uint32_t mipLevel = 0; mipLevel < mipLevels.size() + 1; ++mipLevel)
	{
		Wolf::Extent3D extentForMip = { extent.width >> mipLevel, extent.height >> mipLevel, 1 };

		const uint32_t sliceCountX = std::max(extentForMip.width / maxSliceExtent.width, 1u);
		const uint32_t sliceCountY = std::max(extentForMip.height / maxSliceExtent.height, 1u);

		for (uint32_t sliceX = 0; sliceX < sliceCountX; ++sliceX)
		{
			for (uint32_t sliceY = 0; sliceY < sliceCountY; ++sliceY)
			{
				std::string binFilename = binFolder + "mip" + std::to_string(mipLevel) + "_sliceX" + std::to_string(sliceX) + "_sliceY" + std::to_string(sliceY) + ".bin";
				if (std::filesystem::exists(binFilename))
				{
					std::ifstream input(binFilename, std::ios::in | std::ios::binary);

					uint64_t hash;
					input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
					if (hash == Wolf::HASH_TEXTURE_SET_LOADER_CPP)
					{
						continue;
					}

					Wolf::Debug::sendInfo(binFilename + " found but hash is incorrect");
				}
				else
				{
					Wolf::Debug::sendInfo(binFilename + " not found");
				}

				std::fstream outCacheFile(binFilename, std::ios::out | std::ios::binary);

				/* Hash */
				uint64_t hash = Wolf::HASH_TEXTURE_SET_LOADER_CPP;
				outCacheFile.write(reinterpret_cast<char*>(&hash), sizeof(hash));

				Wolf::Extent3D pixelsToCompressExtent{ std::min(extentForMip.width, maxSliceExtent.width), std::min(extentForMip.height, maxSliceExtent.height), 1 };
				pixelsToCompressExtent.width += 2 * Wolf::VirtualTextureManager::BORDER_SIZE;
				pixelsToCompressExtent.height += 2 * Wolf::VirtualTextureManager::BORDER_SIZE;
				std::vector<PixelType> pixelsToCompress(pixelsToCompressExtent.width * pixelsToCompressExtent.height);

				const std::vector<PixelType>& allPixelsSrc = mipLevel == 0 ? pixels : mipLevels[mipLevel - 1];
				for (uint32_t line = 0; line < pixelsToCompressExtent.height; ++line)
				{
					uint32_t srcY = sliceY * maxSliceExtent.width + line - Wolf::VirtualTextureManager::BORDER_SIZE;
					if (sliceY == 0 && line < Wolf::VirtualTextureManager::BORDER_SIZE)
					{
						srcY = extentForMip.height - Wolf::VirtualTextureManager::BORDER_SIZE + line;
					}
					else if (sliceY == sliceCountY - 1 && line >= pixelsToCompressExtent.height - Wolf::VirtualTextureManager::BORDER_SIZE)
					{
						srcY = line - std::min(extentForMip.width, maxSliceExtent.width) - Wolf::VirtualTextureManager::BORDER_SIZE;
					}

					uint32_t srcX = sliceX * maxSliceExtent.width;

					const PixelType* src = &allPixelsSrc[srcY * extentForMip.width + srcX];
					PixelType* dst = &pixelsToCompress[line * pixelsToCompressExtent.width + Wolf::VirtualTextureManager::BORDER_SIZE];

					memcpy(dst, src, std::min(extentForMip.width, maxSliceExtent.width) * sizeof(PixelType)); // copy without left and right border

					// Left border
					for (uint32_t i = 0; i < Wolf::VirtualTextureManager::BORDER_SIZE; ++i)
					{
						// Left border
						PixelType& leftBorderPixelSrc = pixelsToCompress[line * pixelsToCompressExtent.width + i];
						if (sliceX == 0)
						{
							leftBorderPixelSrc = allPixelsSrc[srcY * extentForMip.width + extentForMip.width - Wolf::VirtualTextureManager::BORDER_SIZE + i];
						}
						else
						{
							leftBorderPixelSrc = allPixelsSrc[srcY * extentForMip.width + srcX - Wolf::VirtualTextureManager::BORDER_SIZE + i];
						}

						// Right border
						PixelType& rightBorderPixelSrc = pixelsToCompress[line * pixelsToCompressExtent.width + pixelsToCompressExtent.width - Wolf::VirtualTextureManager::BORDER_SIZE + i];
						if (sliceX == sliceCountX - 1)
						{
							rightBorderPixelSrc = allPixelsSrc[srcY * extentForMip.width + i];
						}
						else
						{
							rightBorderPixelSrc = allPixelsSrc[srcY * extentForMip.width + srcX + maxSliceExtent.width + i];
						}
					}
				}

				std::vector<CompressionType> compressedBlocks;
				Wolf::ImageCompression::compress(pixelsToCompressExtent, pixelsToCompress, compressedBlocks);

				uint32_t dataBytesCount = static_cast<uint32_t>(compressedBlocks.size() * sizeof(CompressionType));
				outCacheFile.write(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));
				outCacheFile.write(reinterpret_cast<char*>(compressedBlocks.data()), dataBytesCount);

				outCacheFile.close();
			}
		}
	}
}

template <typename PixelType, typename CompressionType>
void TextureSetLoader::createSlicedCacheFromFile(const std::string& filename, bool sRGB, Wolf::ImageCompression::Compression compression, uint32_t imageIdx)
{
	std::string filenameNoExtension = filename.substr(filename.find_last_of("\\") + 1, filename.find_last_of("."));
	std::string folder = filename.substr(0, filename.find_last_of("\\"));
	std::string binFolder = folder + '\\' + filenameNoExtension + "_bin" + "\\";

	std::string binFolderFixed;
	for (const char character : binFolder)
	{
		if (character == '/')
			binFolderFixed += "\\";
		else
			binFolderFixed += character;
	}

	if (!std::filesystem::is_directory(binFolderFixed) || !std::filesystem::exists(binFolderFixed))
	{
		std::filesystem::create_directory(binFolderFixed);
	}

	m_outputFolders[imageIdx] = binFolderFixed;

	// Check 1st file to avoid loading pixels if we don't need
	std::string binFilename = binFolderFixed + "mip0_sliceX0_sliceY0.bin";
	if (std::filesystem::exists(binFilename))
	{
		std::ifstream input(binFilename, std::ios::in | std::ios::binary);

		uint64_t hash;
		input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
		if (hash == Wolf::HASH_TEXTURE_SET_LOADER_CPP)
		{
			return; // we assume cache is ready
		}
	}

	Wolf::Format format = findFormatFromCompression(compression, sRGB);

	std::vector<PixelType> pixels;
	std::vector<std::vector<PixelType>> mipLevels;
	Wolf::Extent3D extent{};
	loadImageFile(filename, format, pixels, mipLevels, extent);

	createSlicedCacheFromData<PixelType, CompressionType>(binFolderFixed, extent, pixels, mipLevels);
}

void TextureSetLoader::createImageFromData(Wolf::Extent3D extent, Wolf::Format format, const unsigned char* pixels, const std::vector<const unsigned char*>& mipLevels, uint32_t idx)
{
	Wolf::CreateImageInfo createImageInfo;
	createImageInfo.extent = extent;
	createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	createImageInfo.format = format;
	createImageInfo.mipLevelCount = static_cast<uint32_t>(mipLevels.size()) + 1;
	createImageInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
	m_outputImages[idx].reset(Wolf::Image::createImage(createImageInfo));
	pushDataToGPUImage(pixels, m_outputImages[idx].createNonOwnerResource(), Wolf::Image::SampledInFragmentShader());

	for (uint32_t mipLevel = 1; mipLevel < mipLevels.size() + 1; ++mipLevel)
	{
		pushDataToGPUImage(mipLevels[mipLevel - 1], m_outputImages[idx].createNonOwnerResource(), Wolf::Image::SampledInFragmentShader(mipLevel), mipLevel);
	}
}

std::string extractFilenameFromPath(const std::string& fullPath)
{
	return fullPath.substr(fullPath.find_last_of("/\\") + 1);
}

std::string extractFolderFromFullPath(const std::string& fullPath)
{
	return fullPath.substr(0, fullPath.find_last_of("/\\"));
}

void TextureSetLoader::createCombinedTexture(const TextureSetFileInfoGGX& textureSet)
{
	bool useCache = m_useCache && !textureSet.roughness.empty();

	std::string combinedTexturePath = extractFolderFromFullPath(textureSet.roughness) + "combined_";
	combinedTexturePath += extractFilenameFromPath(textureSet.roughness) + "_";
	combinedTexturePath += extractFilenameFromPath(textureSet.metalness) + "_";
	combinedTexturePath += extractFilenameFromPath(textureSet.ao) + "_";
	combinedTexturePath += extractFilenameFromPath(textureSet.anisoStrength);

	if (useCache)
	{
		if (createImageFileFromCache(combinedTexturePath, false, Wolf::ImageCompression::Compression::BC3, 2))
		{
			return;
		}
	}

	std::string binFolderForSlices;
	if (Wolf::g_configuration->getUseVirtualTexture())
	{
		std::string roughnessFilenameNoExtension = textureSet.roughness.substr(textureSet.roughness.find_last_of("\\") + 1, textureSet.roughness.find_last_of("."));
		std::string metalnessFilenameNoExtension = textureSet.metalness.substr(textureSet.metalness.find_last_of("\\") + 1, textureSet.metalness.find_last_of("."));
		std::string aoFilenameNoExtension = textureSet.ao.substr(textureSet.ao.find_last_of("\\") + 1, textureSet.ao.find_last_of("."));
		std::string anisoFilenameNoExtension = textureSet.anisoStrength.substr(textureSet.anisoStrength.find_last_of("\\") + 1, textureSet.anisoStrength.find_last_of("."));
		std::string folder = textureSet.roughness.substr(0, textureSet.roughness.find_last_of("\\"));
		std::string binFolder = folder + '\\' + roughnessFilenameNoExtension + "_" + metalnessFilenameNoExtension + "_" + aoFilenameNoExtension + "_" + anisoFilenameNoExtension + "_bin" + "\\";

		for (const char character : binFolder)
		{
			if (character == '/')
				binFolderForSlices += "\\";
			else
				binFolderForSlices += character;
		}

		if (!std::filesystem::is_directory(binFolderForSlices) || !std::filesystem::exists(binFolderForSlices))
		{
			std::filesystem::create_directory(binFolderForSlices);
		}

		m_outputFolders[2] = binFolderForSlices;

		// Check 1st file to avoid loading pixels if we don't need
		std::string binFilename = binFolderForSlices + "mip0_sliceX0_sliceY0.bin";
		if (std::filesystem::exists(binFilename))
		{
			std::ifstream input(binFilename, std::ios::in | std::ios::binary);

			uint64_t hash;
			input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
			if (hash == Wolf::HASH_TEXTURE_SET_LOADER_CPP)
			{
				return; // we assume cache is ready
			}
		}
	}

	std::vector<Wolf::ImageCompression::RGBA8> combinedRoughnessMetalnessAOAniso;
	std::vector<std::vector<Wolf::ImageCompression::RGBA8>> combinedRoughnessMetalnessAOMipLevels;
	Wolf::Extent3D combinedRoughnessMetalnessAOExtent = { 1, 1, 1};
	combinedRoughnessMetalnessAOAniso.resize(1);

	// Roughness
	if (!textureSet.roughness.empty())
	{
		Wolf::Debug::sendInfo("Loading roughness " + textureSet.roughness);
		Wolf::Timer albedoTimer("Loading roughness " + textureSet.roughness);

		std::vector<Wolf::ImageCompression::RGBA8> pixels;
		std::vector<std::vector<Wolf::ImageCompression::RGBA8>> mipLevels;
		loadImageFile(textureSet.roughness, Wolf::Format::R8G8B8A8_UNORM, pixels, mipLevels, combinedRoughnessMetalnessAOExtent);

		combinedRoughnessMetalnessAOAniso.resize(pixels.size());
		for (uint32_t i = 0; i < pixels.size(); ++i)
		{
			combinedRoughnessMetalnessAOAniso[i].r = pixels[i].r;
		}
		combinedRoughnessMetalnessAOMipLevels.resize(mipLevels.size());
		for (uint32_t i = 0; i < mipLevels.size(); ++i)
		{
			combinedRoughnessMetalnessAOMipLevels[i].resize(mipLevels[i].size());
			for (uint32_t j = 0; j < mipLevels[i].size(); ++j)
			{
				combinedRoughnessMetalnessAOMipLevels[i][j].r = mipLevels[i][j].r;
			}
		}
	}

	// Metalness
	if (!textureSet.metalness.empty())
	{
		Wolf::Debug::sendInfo("Loading metalness " + textureSet.metalness);
		Wolf::Timer albedoTimer("Loading metalness " + textureSet.metalness);

		std::vector<Wolf::ImageCompression::RGBA8> pixels;
		std::vector<std::vector<Wolf::ImageCompression::RGBA8>> mipLevels;
		Wolf::Extent3D extent;
		loadImageFile(textureSet.metalness, Wolf::Format::R8G8B8A8_UNORM, pixels, mipLevels, extent);

		if (extent.width != combinedRoughnessMetalnessAOExtent.width || extent.height != combinedRoughnessMetalnessAOExtent.height)
		{
			Wolf::Debug::sendWarning("Metalness has not same resolution than roughness, this is not supported and will be set to default value");
		}
		else
		{
			for (uint32_t i = 0; i < pixels.size(); ++i)
			{
				combinedRoughnessMetalnessAOAniso[i].g = pixels[i].r;
			}
			for (uint32_t i = 0; i < mipLevels.size(); ++i)
			{
				for (uint32_t j = 0; j < mipLevels[i].size(); ++j)
				{
					combinedRoughnessMetalnessAOMipLevels[i][j].g = mipLevels[i][j].r;
				}
			}
		}
	}

	// AO
	if (!textureSet.ao.empty())
	{
		Wolf::Debug::sendInfo("Loading ao " + textureSet.ao);
		Wolf::Timer albedoTimer("Loading ao " + textureSet.ao);

		std::vector<Wolf::ImageCompression::RGBA8> pixels;
		std::vector<std::vector<Wolf::ImageCompression::RGBA8>> mipLevels;
		Wolf::Extent3D extent;
		loadImageFile(textureSet.ao, Wolf::Format::R8G8B8A8_UNORM, pixels, mipLevels, extent);

		if (extent.width != combinedRoughnessMetalnessAOExtent.width || extent.height != combinedRoughnessMetalnessAOExtent.height)
		{
			Wolf::Debug::sendWarning("AO has not same resolution than roughness, this is not supported and will be set to default value");
		}
		else
		{
			for (uint32_t i = 0; i < pixels.size(); ++i)
			{
				combinedRoughnessMetalnessAOAniso[i].b = pixels[i].r;
			}
			for (uint32_t i = 0; i < mipLevels.size(); ++i)
			{
				for (uint32_t j = 0; j < mipLevels[i].size(); ++j)
				{
					combinedRoughnessMetalnessAOMipLevels[i][j].b = mipLevels[i][j].r;
				}
			}
		}
	}

	// Anisotropy strength
	if (!textureSet.anisoStrength.empty())
	{
		Wolf::Debug::sendInfo("Loading anisoStrength " + textureSet.anisoStrength);
		Wolf::Timer albedoTimer("Loading anisoStrength " + textureSet.anisoStrength);

		std::vector<Wolf::ImageCompression::RGBA8> pixels;
		std::vector<std::vector<Wolf::ImageCompression::RGBA8>> mipLevels;
		Wolf::Extent3D extent;
		loadImageFile(textureSet.anisoStrength, Wolf::Format::R8G8B8A8_UNORM, pixels, mipLevels, extent);

		if (extent.width != combinedRoughnessMetalnessAOExtent.width || extent.height != combinedRoughnessMetalnessAOExtent.height)
		{
			Wolf::Debug::sendWarning("Anisotropic strength has not same resolution than roughness, this is not supported and will be set to default value");
		}
		else
		{
			for (uint32_t i = 0; i < pixels.size(); ++i)
			{
				combinedRoughnessMetalnessAOAniso[i].a = pixels[i].r;
			}
			for (uint32_t i = 0; i < mipLevels.size(); ++i)
			{
				for (uint32_t j = 0; j < mipLevels[i].size(); ++j)
				{
					combinedRoughnessMetalnessAOMipLevels[i][j].a = mipLevels[i][j].r;
				}
			}
		}
	}

	if (Wolf::g_configuration->getUseVirtualTexture())
	{
		createSlicedCacheFromData<Wolf::ImageCompression::RGBA8, Wolf::ImageCompression::BC3>(binFolderForSlices, combinedRoughnessMetalnessAOExtent, combinedRoughnessMetalnessAOAniso, combinedRoughnessMetalnessAOMipLevels);
	}
	else if (!textureSet.roughness.empty())
	{
		Wolf::Debug::sendInfo("Creating combined texture");
		Wolf::Timer albedoTimer("Creating combined texture");

		std::fstream outCacheFile(combinedTexturePath + ".bin", std::ios::out | std::ios::binary);

		/* Hash */
		uint64_t hash = Wolf::HASH_TEXTURE_SET_LOADER_CPP;
		outCacheFile.write(reinterpret_cast<char*>(&hash), sizeof(hash));
		outCacheFile.write(reinterpret_cast<char*>(&combinedRoughnessMetalnessAOExtent), sizeof(combinedRoughnessMetalnessAOExtent));

		compressAndCreateImage<Wolf::ImageCompression::BC3>(combinedRoughnessMetalnessAOMipLevels, combinedRoughnessMetalnessAOAniso, combinedRoughnessMetalnessAOExtent, Wolf::Format::BC3_UNORM_BLOCK,
			combinedTexturePath, outCacheFile, 2);
	}
}

template <typename CompressionType, typename PixelType>
void TextureSetLoader::compressAndCreateImage(std::vector<std::vector<PixelType>>& mipLevels, const std::vector<PixelType>& pixels, Wolf::Extent3D& extent, Wolf::Format format, const std::string& filename, std::fstream& outCacheFile, uint32_t imageIdx)
{
	std::vector<CompressionType> compressedBlocks;
	std::vector<std::vector<CompressionType>> mipBlocks(mipLevels.size());
	Wolf::ImageCompression::compress(extent, pixels, compressedBlocks);
	uint32_t mipWidth = extent.width / 2;
	uint32_t mipHeight = extent.height / 2;
	for (uint32_t i = 0; i < mipLevels.size(); ++i)
	{
		// Can't compress if size can't be divided by 4 (as we take 4x4 blocks)
		if (mipWidth % 4 != 0 || mipHeight % 4 != 0)
		{
			Wolf::Debug::sendWarning("Image " + filename + " resolution is not a power of 2, not all mips are generated");
			mipLevels.resize(i);
			mipBlocks.resize(i);
			break;
		}

		Wolf::ImageCompression::compress({ mipWidth, mipHeight, 1 }, mipLevels[i], mipBlocks[i]);

		mipWidth /= 2;
		mipHeight /= 2;
	}

	std::vector<const unsigned char*> mipsData(mipBlocks.size());
	for (uint32_t i = 0; i < mipBlocks.size(); ++i)
	{
		mipsData[i] = reinterpret_cast<const unsigned char*>(mipBlocks[i].data());
	}
	createImageFromData(extent, format, reinterpret_cast<const unsigned char*>(compressedBlocks.data()), mipsData, imageIdx);

	// Add to cache
	uint32_t dataBytesCount = static_cast<uint32_t>(compressedBlocks.size() * sizeof(CompressionType));
	outCacheFile.write(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));
	outCacheFile.write(reinterpret_cast<char*>(compressedBlocks.data()), dataBytesCount);

	uint32_t mipsCount = static_cast<uint32_t>(mipBlocks.size());
	outCacheFile.write(reinterpret_cast<char*>(&mipsCount), sizeof(mipsCount));

	for (uint32_t i = 0; i < mipsCount; ++i)
	{
		dataBytesCount = static_cast<uint32_t>(mipBlocks[i].size() * sizeof(CompressionType));
		outCacheFile.write(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));
		outCacheFile.write(reinterpret_cast<char*>(mipBlocks[i].data()), dataBytesCount);
	}
}
