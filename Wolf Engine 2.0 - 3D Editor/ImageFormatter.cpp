#include "ImageFormatter.h"

#include <Configuration.h>
#include <ImageFileLoader.h>
#include <GPUDataTransfersManager.h>

#include "MipMapGenerator.h"

ImageFormatter::ImageFormatter(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& filename, const std::string& slicesFolder, Wolf::Format finalFormat, bool canBeVirtualized,
	bool keepDataOnCPU, bool loadMips) : m_keepDataOnCPU(keepDataOnCPU), m_loadMips(loadMips), m_editorPushDataToGPU(editorPushDataToGPU)
{
    if (canBeVirtualized && Wolf::g_configuration->getUseVirtualTexture())
    {
        if (finalFormat == Wolf::Format::BC1_RGB_SRGB_BLOCK)
        {
            createSlicedCacheFromFile<Wolf::ImageCompression::RGBA8, Wolf::ImageCompression::BC1>(filename, slicesFolder, Wolf::isSRGBFormat(finalFormat), finalFormat);
        }
    	else if (finalFormat == Wolf::Format::BC5_UNORM_BLOCK)
    	{
    		createSlicedCacheFromFile<Wolf::ImageCompression::RG32F, Wolf::ImageCompression::BC5>(filename, slicesFolder, Wolf::isSRGBFormat(finalFormat), finalFormat);
    	}
    	else if (finalFormat == Wolf::Format::BC3_UNORM_BLOCK)
    	{
    		createSlicedCacheFromFile<Wolf::ImageCompression::RGBA8, Wolf::ImageCompression::BC3>(filename, slicesFolder, Wolf::isSRGBFormat(finalFormat), finalFormat);
    	}
    	else if (finalFormat == Wolf::Format::R8G8B8A8_UNORM)
    	{
    		createSlicedCacheFromFile<Wolf::ImageCompression::RGBA8, Wolf::ImageCompression::RGBA8>(filename, slicesFolder, Wolf::isSRGBFormat(finalFormat), finalFormat);
    	}
        else
        {
            Wolf::Debug::sendCriticalError("Unhandled case");
        }
    }
    else
    {
        if (finalFormat == Wolf::Format::BC1_RGB_SRGB_BLOCK || finalFormat == Wolf::Format::BC3_UNORM_BLOCK || finalFormat == Wolf::Format::R8G8B8A8_UNORM || finalFormat == Wolf::Format::R8G8B8A8_SRGB)
        {
            if (!createImageFileFromCache(filename, finalFormat))
            {
                createImageFileFromSource<Wolf::ImageCompression::RGBA8>(filename, finalFormat);
            }
        }
    	else if (finalFormat == Wolf::Format::BC5_UNORM_BLOCK)
    	{
    		if (!createImageFileFromCache(filename, finalFormat))
    		{
    			createImageFileFromSource<Wolf::ImageCompression::RG32F>(filename, finalFormat);
    		}
    	}
    	else if (finalFormat == Wolf::Format::R32G32B32A32_SFLOAT)
    	{
    		if (!createImageFileFromCache(filename, finalFormat))
    		{
    			createImageFileFromSource<Wolf::ImageCompression::RGBA32F>(filename, finalFormat);
    		}
    	}
        else
        {
            Wolf::Debug::sendCriticalError("Unhandled case");
        }
    }
}

ImageFormatter::ImageFormatter(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::vector<Wolf::ImageCompression::RGBA8>& data, std::vector<std::vector<Wolf::ImageCompression::RGBA8>>& mipLevels,
	Wolf::Extent3D extent, const std::string& binFolder, const std::string& path, Wolf::Format finalFormat, bool canBeVirtualized, bool keepDataOnCPU)
: m_keepDataOnCPU(keepDataOnCPU), m_editorPushDataToGPU(editorPushDataToGPU)
{
	if (finalFormat != Wolf::Format::BC3_UNORM_BLOCK)
	{
		Wolf::Debug::sendCriticalError("Unsupported format");
	}

	if (canBeVirtualized && Wolf::g_configuration->getUseVirtualTexture())
	{
		std::string binFolderForSlices;
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

		m_slicesFolder = binFolderForSlices;
		createSlicedCacheFromData<Wolf::ImageCompression::RGBA8, Wolf::ImageCompression::BC3>(binFolderForSlices, extent, data, mipLevels);
	}
	else
	{
		std::fstream outCacheFile(path + ".bin", std::ios::out | std::ios::binary);

		/* Hash */
		uint64_t hash = HASH;
		outCacheFile.write(reinterpret_cast<char*>(&hash), sizeof(hash));
		outCacheFile.write(reinterpret_cast<char*>(&extent), sizeof(Wolf::Extent3D));

		compressAndCreateImage<Wolf::ImageCompression::BC3>(mipLevels, data, extent, Wolf::Format::BC3_UNORM_BLOCK, path, outCacheFile);
	}
}

void ImageFormatter::transferImageTo(Wolf::ResourceUniqueOwner<Wolf::Image>& output)
{
	return output.transferFrom(m_outputImage);
}

const uint8_t* ImageFormatter::getPixels() const
{
	return m_pixels.data();
}

bool ImageFormatter::isCacheAvailable(const std::string& filename, const std::string& binFolder, bool canBeVirtualized)
{
	if (canBeVirtualized && Wolf::g_configuration->getUseVirtualTexture())
	{
		std::string binFolderForSlices;
		for (const char character : binFolder)
		{
			if (character == '/')
				binFolderForSlices += "\\";
			else
				binFolderForSlices += character;
		}

		// Check 1st file to avoid loading pixels if we don't need
		std::string binFilename = binFolderForSlices + "mip0_sliceX0_sliceY0.bin";
		if (std::filesystem::exists(binFilename))
		{
			std::ifstream input(binFilename, std::ios::in | std::ios::binary);

			uint64_t hash;
			input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
			if (hash == HASH)
			{
				return true; // we assume cache is ready
			}
		}
	}
	else
	{
		std::string binFilename = filename + ".bin";
		if (std::filesystem::exists(binFilename))
		{
			std::ifstream input(binFilename, std::ios::in | std::ios::binary);

			uint64_t hash;
			input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
			if (hash != HASH)
			{
				// Cache found but hash is incorrect
				return false;
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

Wolf::ImageCompression::Compression ImageFormatter::findCompressionFromFormat(Wolf::Format format)
{
	if (format == Wolf::Format::BC1_RGB_SRGB_BLOCK)
		return Wolf::ImageCompression::Compression::BC1;
	else if (format == Wolf::Format::BC3_SRGB_BLOCK || format == Wolf::Format::BC3_UNORM_BLOCK)
		return Wolf::ImageCompression::Compression::BC3;
	else if (format == Wolf::Format::BC5_UNORM_BLOCK)
		return Wolf::ImageCompression::Compression::BC5;
	else
		return Wolf::ImageCompression::Compression::NO_COMPRESSION;
}

Wolf::Format ImageFormatter::findUncompressedFormat(Wolf::Format format)
{
	if (format == Wolf::Format::BC1_RGB_SRGB_BLOCK)
		return Wolf::Format::R8G8B8A8_SRGB;
	else if (format == Wolf::Format::BC3_SRGB_BLOCK || format == Wolf::Format::BC3_UNORM_BLOCK)
		return Wolf::Format::R8G8B8A8_UNORM;
	else if (format == Wolf::Format::BC5_UNORM_BLOCK)
		return Wolf::Format::R32G32_SFLOAT;
	else
		return format;
}

bool ImageFormatter::createImageFileFromCache(const std::string& filename, Wolf::Format format)
{
    std::string binFilename = filename + ".bin";
	if (std::filesystem::exists(binFilename))
	{
		std::ifstream input(binFilename, std::ios::in | std::ios::binary);

		uint64_t hash;
		input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
		if (hash != HASH)
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

		createImageFromData(extent, format, data.data(), mipsDataPtr);

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
		createImageFromData(extent, format, imageFileLoader.getPixels(), mipPtrs);

		return true;
	}

	Wolf::Debug::sendInfo("Cache not found");
	return false;
}

void ImageFormatter::loadImageFile(const std::string& filename, Wolf::Format format, bool loadMips, std::vector<Wolf::ImageCompression::RGBA8>& pixels, std::vector<std::vector<Wolf::ImageCompression::RGBA8>>& mipLevels, Wolf::Extent3D& outExtent)
{
	const Wolf::ImageFileLoader imageFileLoader(filename);

	if (imageFileLoader.getCompression() != Wolf::ImageCompression::Compression::NO_COMPRESSION)
	{
		Wolf::ImageCompression::uncompressImage(imageFileLoader.getCompression(), imageFileLoader.getPixels(), { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()) }, pixels);
	}
	else
	{
		pixels.assign(reinterpret_cast<Wolf::ImageCompression::RGBA8*>(imageFileLoader.getPixels()),
			reinterpret_cast<Wolf::ImageCompression::RGBA8*>(imageFileLoader.getPixels()) + static_cast<size_t>(imageFileLoader.getWidth()) * imageFileLoader.getHeight());
	}

	if (loadMips)
	{
		const Wolf::MipMapGenerator mipmapGenerator(reinterpret_cast<const unsigned char*>(pixels.data()), { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()) }, format);

		mipLevels.resize(mipmapGenerator.getMipLevelCount() - 1);
		for (uint32_t mipLevel = 1; mipLevel < mipmapGenerator.getMipLevelCount(); ++mipLevel)
		{
			const std::vector<unsigned char>& mipPixels = mipmapGenerator.getMipLevel(mipLevel);
			const Wolf::ImageCompression::RGBA8* mipPixelsAsColor = reinterpret_cast<const Wolf::ImageCompression::RGBA8*>(mipPixels.data());
			mipLevels[mipLevel - 1] = std::vector<Wolf::ImageCompression::RGBA8>(mipPixelsAsColor, mipPixelsAsColor + mipPixels.size() / sizeof(Wolf::ImageCompression::RGBA8));
		}
	}

	outExtent = { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()), 1 };
}

void ImageFormatter::loadImageFile(const std::string& filename, Wolf::Format format, bool loadMips, std::vector<Wolf::ImageCompression::RG32F>& pixels, std::vector<std::vector<Wolf::ImageCompression::RG32F>>& mipLevels, Wolf::Extent3D& outExtent)
{
	const Wolf::ImageFileLoader imageFileLoader(filename, false);

	if (imageFileLoader.getCompression() != Wolf::ImageCompression::Compression::NO_COMPRESSION)
	{
		Wolf::Debug::sendCriticalError("Format should be uncompressed here");
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

	if (loadMips)
	{
		const Wolf::MipMapGenerator mipmapGenerator(reinterpret_cast<const unsigned char*>(pixels.data()), { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()) }, format);

		mipLevels.resize(mipmapGenerator.getMipLevelCount() - 1);
		for (uint32_t mipLevel = 1; mipLevel < mipmapGenerator.getMipLevelCount(); ++mipLevel)
		{
			const std::vector<unsigned char>& mipPixels = mipmapGenerator.getMipLevel(mipLevel);
			const Wolf::ImageCompression::RG32F* mipPixelsAsColor = reinterpret_cast<const Wolf::ImageCompression::RG32F*>(mipPixels.data());
			mipLevels[mipLevel - 1] = std::vector<Wolf::ImageCompression::RG32F>(mipPixelsAsColor, mipPixelsAsColor + mipPixels.size() / sizeof(Wolf::ImageCompression::RG32F));
		}
	}

	outExtent = { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()), 1 };
}

void ImageFormatter::loadImageFile(const std::string& filename, Wolf::Format format, bool loadMips, std::vector<Wolf::ImageCompression::RGBA32F>& pixels, std::vector<std::vector<Wolf::ImageCompression::RGBA32F>>& mipLevels, Wolf::Extent3D& outExtent)
{
	const Wolf::ImageFileLoader imageFileLoader(filename, true);

	if (imageFileLoader.getCompression() != Wolf::ImageCompression::Compression::NO_COMPRESSION)
	{
		Wolf::Debug::sendCriticalError("Unhandled");
	}
	else
	{
		pixels.assign(reinterpret_cast<Wolf::ImageCompression::RGBA32F*>(imageFileLoader.getPixels()),
			reinterpret_cast<Wolf::ImageCompression::RGBA32F*>(imageFileLoader.getPixels()) + static_cast<size_t>(imageFileLoader.getWidth()) * imageFileLoader.getHeight());
	}

	if (loadMips)
	{
		const Wolf::MipMapGenerator mipmapGenerator(reinterpret_cast<const unsigned char*>(pixels.data()), { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()) }, format);

		mipLevels.resize(mipmapGenerator.getMipLevelCount() - 1);
		for (uint32_t mipLevel = 1; mipLevel < mipmapGenerator.getMipLevelCount(); ++mipLevel)
		{
			const std::vector<unsigned char>& mipPixels = mipmapGenerator.getMipLevel(mipLevel);
			const Wolf::ImageCompression::RGBA32F* mipPixelsAsColor = reinterpret_cast<const Wolf::ImageCompression::RGBA32F*>(mipPixels.data());
			mipLevels[mipLevel - 1] = std::vector<Wolf::ImageCompression::RGBA32F>(mipPixelsAsColor, mipPixelsAsColor + mipPixels.size() / sizeof(Wolf::ImageCompression::RGBA8));
		}
	}

	outExtent = { (imageFileLoader.getWidth()), (imageFileLoader.getHeight()), 1 };
}

void ImageFormatter::createImageFromData(Wolf::Extent3D extent, Wolf::Format format, const uint8_t* pixels, const std::vector<const unsigned char*>& mipLevels)
{
	Wolf::CreateImageInfo createImageInfo;
	createImageInfo.extent = extent;
	createImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::COLOR;
	createImageInfo.format = format;
	createImageInfo.mipLevelCount = static_cast<uint32_t>(mipLevels.size()) + 1;
	createImageInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
	m_outputImage.reset(Wolf::Image::createImage(createImageInfo));

	{
		Wolf::GPUDataTransfersManagerInterface::PushDataToGPUImageInfo pushDataToGpuImageInfo(pixels, m_outputImage.createNonOwnerResource(), Wolf::Image::SampledInFragmentShader());
		m_editorPushDataToGPU->pushDataToGPUImage(pushDataToGpuImageInfo);
	}

	for (uint32_t mipLevel = 1; mipLevel < mipLevels.size() + 1; ++mipLevel)
	{
		Wolf::GPUDataTransfersManagerInterface::PushDataToGPUImageInfo pushDataToGpuImageInfo(mipLevels[mipLevel - 1], m_outputImage.createNonOwnerResource(), Wolf::Image::SampledInFragmentShader(mipLevel), mipLevel);
		m_editorPushDataToGPU->pushDataToGPUImage(pushDataToGpuImageInfo);
	}

	if (m_keepDataOnCPU)
	{
		uint32_t copySize = extent.width * extent.height * extent.depth * m_outputImage->getBPP();
		m_pixels.resize(copySize);
		memcpy(m_pixels.data(), pixels, copySize);
	}
}
