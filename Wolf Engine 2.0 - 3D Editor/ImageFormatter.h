#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <ConfigurationHelper.h>
#include <Extents.h>
#include <ImageCompression.h>
#include <VirtualTextureManager.h>

#include "CodeFileHashes.h"
#include "EditorGPUDataTransfersManager.h"

class ImageFormatter
{
public:
	ImageFormatter(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& filename, const std::string& slicesFolder, Wolf::Format finalFormat, bool canBeVirtualized, bool keepDataOnCPU,
		bool loadMips);
	ImageFormatter(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::vector<Wolf::ImageCompression::RGBA8>& data, std::vector<std::vector<Wolf::ImageCompression::RGBA8>>& mipLevels,
		Wolf::Extent3D extent, const std::string& binFolder, const std::string& path, Wolf::Format finalFormat, bool canBeVirtualized, bool keepDataOnCPU);

	void transferImageTo(Wolf::ResourceUniqueOwner<Wolf::Image>& output);
	std::string getSlicesFolder() const { return m_slicesFolder; }
	const uint8_t* getPixels() const;

	static bool isCacheAvailable(const std::string& filename, const std::string& binFolder, bool canBeVirtualized);
	static void loadImageFile(const std::string& filename, Wolf::Format format, bool loadMips, std::vector<Wolf::ImageCompression::RGBA8>& pixels, std::vector<std::vector<Wolf::ImageCompression::RGBA8>>& mipLevels, Wolf::Extent3D& outExtent);
	static void loadImageFile(const std::string& filename, Wolf::Format format, bool loadMips, std::vector<Wolf::ImageCompression::RG32F>& pixels, std::vector<std::vector<Wolf::ImageCompression::RG32F>>& mipLevels, Wolf::Extent3D& outExtent);
	static void loadImageFile(const std::string& filename, Wolf::Format format, bool loadMips, std::vector<Wolf::ImageCompression::RGBA32F>& pixels, std::vector<std::vector<Wolf::ImageCompression::RGBA32F>>& mipLevels, Wolf::Extent3D& outExtent);

private:
	static constexpr uint64_t HASH = Wolf::HASH_IMAGE_FORMATTER_H ^ Wolf::HASH_IMAGE_FORMATTER_CPP;

	bool m_keepDataOnCPU;
	std::vector<uint8_t> m_pixels;
	bool m_loadMips;
	Wolf::ResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;

	static Wolf::ImageCompression::Compression findCompressionFromFormat(Wolf::Format format);
	static Wolf::Format findUncompressedFormat(Wolf::Format format);

	// No virtual texture
	Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;

	[[nodiscard]] bool createImageFileFromCache(const std::string& filename, Wolf::Format format);
	template <typename PixelType>
	void createImageFileFromSource(const std::string& filename, Wolf::Format format);

	void createImageFromData(Wolf::Extent3D extent, Wolf::Format format, const uint8_t* pixels, const std::vector<const unsigned char*>& mipLevels);
	template <typename CompressionType, typename PixelType>
	void compressAndCreateImage(std::vector<std::vector<PixelType>>& mipLevels, const std::vector<PixelType>& pixels, Wolf::Extent3D& extent, Wolf::Format format, const std::string& filename, std::fstream& outCacheFile);

    // Virtual texture
	std::string m_slicesFolder;

    template <typename PixelType, typename CompressionType>
    void createSlicedCacheFromData(const std::string& binFolder, Wolf::Extent3D extent, const std::vector<PixelType>& pixels, const std::vector<std::vector<PixelType>>& mipLevels);
    template <typename PixelType, typename CompressionType>
    void createSlicedCacheFromFile(const std::string& filename, const std::string& slicesFolder, bool sRGB, Wolf::Format format);
};

template <typename PixelType>
void ImageFormatter::createImageFileFromSource(const std::string& filename, Wolf::Format format)
{
	std::vector<PixelType> pixels;
	std::vector<std::vector<PixelType>> mipLevels;
	Wolf::Extent3D extent;
	Wolf::Format uncompressedFormat = findUncompressedFormat(format);
	loadImageFile(filename, uncompressedFormat, m_loadMips, pixels, mipLevels, extent);

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
	uint64_t hash = HASH;
	outCacheFile.write(reinterpret_cast<char*>(&hash), sizeof(hash));
	outCacheFile.write(reinterpret_cast<char*>(&extent), sizeof(extent));

	Wolf::ImageCompression::Compression compression = findCompressionFromFormat(format);
	if (compression == Wolf::ImageCompression::Compression::BC1)
		compressAndCreateImage<Wolf::ImageCompression::BC1>(mipLevels, pixels, extent, Wolf::Format::BC1_RGB_SRGB_BLOCK, filename, outCacheFile);
	else if (compression == Wolf::ImageCompression::Compression::BC3)
		compressAndCreateImage<Wolf::ImageCompression::BC3>(mipLevels, pixels, extent, Wolf::Format::BC3_SRGB_BLOCK, filename, outCacheFile);
	else if (compression == Wolf::ImageCompression::Compression::BC5)
		compressAndCreateImage<Wolf::ImageCompression::BC5>(mipLevels, pixels, extent, Wolf::Format::BC5_UNORM_BLOCK, filename, outCacheFile);
	else if (compression == Wolf::ImageCompression::Compression::NO_COMPRESSION)
	{
		std::vector<const uint8_t*> rawMipLevels(mipLevels.size());
		for (uint32_t i = 0; i < mipLevels.size(); ++i)
		{
			rawMipLevels[i] = reinterpret_cast<const uint8_t*>(mipLevels[i].data());
		}
		createImageFromData(extent, format, reinterpret_cast<const uint8_t*>(pixels.data()), rawMipLevels);

		// Add to cache
		uint32_t dataBytesCount = static_cast<uint32_t>(pixels.size() * sizeof(PixelType));
		outCacheFile.write(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));
		outCacheFile.write(reinterpret_cast<char*>(pixels.data()), dataBytesCount);

		uint32_t mipsCount = static_cast<uint32_t>(mipLevels.size());
		outCacheFile.write(reinterpret_cast<char*>(&mipsCount), sizeof(mipsCount));

		for (uint32_t i = 0; i < mipsCount; ++i)
		{
			dataBytesCount = static_cast<uint32_t>(mipLevels[i].size() * sizeof(PixelType));
			outCacheFile.write(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));
			outCacheFile.write(reinterpret_cast<char*>(mipLevels[i].data()), dataBytesCount);
		}
	}
	else
		Wolf::Debug::sendError("Requested compression is not available");

	outCacheFile.close();
}

template <typename CompressionType, typename PixelType>
void ImageFormatter::compressAndCreateImage(std::vector<std::vector<PixelType>>& mipLevels, const std::vector<PixelType>& pixels, Wolf::Extent3D& extent, Wolf::Format format, const std::string& filename, std::fstream& outCacheFile)
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
	createImageFromData(extent, format, reinterpret_cast<const unsigned char*>(compressedBlocks.data()), mipsData);

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

template <typename PixelType, typename CompressionType>
void ImageFormatter::createSlicedCacheFromData(const std::string& binFolder, Wolf::Extent3D extent, const std::vector<PixelType>& pixels, const std::vector<std::vector<PixelType>>& mipLevels)
{
    if (extent.depth != 1 ||
		(extent.width > Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE && (extent.width % Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE) != 0) ||
		(extent.height > Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE && (extent.height % Wolf::VirtualTextureManager::VIRTUAL_PAGE_SIZE) != 0))
	{
		Wolf::Debug::sendCriticalError("Wrong texture extent for virtual texture slicing: width/height must be a multiple of VIRTUAL_PAGE_SIZE when larger than a page, and depth must be 1");
	}

	if (extent.width == 0 || extent.height == 0)
	{
		Wolf::Debug::sendCriticalError("Wrong width or height");
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
					if (hash == HASH)
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
				uint64_t hash = HASH;
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

				if (std::is_same<CompressionType, Wolf::ImageCompression::RGBA8>::value)
				{
					uint32_t dataBytesCount = static_cast<uint32_t>(pixelsToCompress.size() * sizeof(CompressionType));
					outCacheFile.write(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));
					outCacheFile.write(reinterpret_cast<char*>(pixelsToCompress.data()), dataBytesCount);
				}
				else
				{
					std::vector<CompressionType> compressedBlocks;
					Wolf::ImageCompression::compress(pixelsToCompressExtent, pixelsToCompress, compressedBlocks);

					uint32_t dataBytesCount = static_cast<uint32_t>(compressedBlocks.size() * sizeof(CompressionType));
					outCacheFile.write(reinterpret_cast<char*>(&dataBytesCount), sizeof(dataBytesCount));
					outCacheFile.write(reinterpret_cast<char*>(compressedBlocks.data()), dataBytesCount);
				}


				outCacheFile.close();
			}
		}
	}
}

template <typename PixelType, typename CompressionType>
void ImageFormatter::createSlicedCacheFromFile(const std::string& filename, const std::string& slicesFolder, bool sRGB, Wolf::Format format)
{
	m_slicesFolder= slicesFolder;

	if (m_slicesFolder.empty())
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

		m_slicesFolder = binFolderFixed;
	}

	// Check 1st file to avoid loading pixels if we don't need
	std::string binFilename = m_slicesFolder + "mip0_sliceX0_sliceY0.bin";
	if (std::filesystem::exists(binFilename) && !m_keepDataOnCPU) // need to re-read the file if we want data on CPU
	{
		std::ifstream input(binFilename, std::ios::in | std::ios::binary);

		uint64_t hash;
		input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
		if (hash == HASH)
		{
			return; // we assume cache is ready
		}
	}

	Wolf::Format uncompressedFormat = findUncompressedFormat(format);

	std::vector<PixelType> pixels;
	std::vector<std::vector<PixelType>> mipLevels;
	Wolf::Extent3D extent{};
	loadImageFile(filename, uncompressedFormat, m_loadMips, pixels, mipLevels, extent);

	createSlicedCacheFromData<PixelType, CompressionType>(m_slicesFolder, extent, pixels, mipLevels);

	if (m_keepDataOnCPU)
	{
		m_pixels.resize(pixels.size() * sizeof(PixelType));
		memcpy(m_pixels.data(), pixels.data(), pixels.size() * sizeof(PixelType));
	}
}
