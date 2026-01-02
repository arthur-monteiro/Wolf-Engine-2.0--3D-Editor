#pragma once

#include <string>

#include "AssetId.h"
#include "Image.h"
#include "ImageCompression.h"
#include "MaterialsGPUManager.h"
#include "ResourceNonOwner.h"
#include "ResourceReference.h"

class AssetManager;

class TextureSetLoader
{
public:
	enum class InputTextureSetLayout
	{
		NO_MATERIAL,
		EACH_TEXTURE_A_FILE
	};

	struct OutputLayout
	{
		Wolf::ImageCompression::Compression albedoCompression;
		Wolf::ImageCompression::Compression normalCompression;
	};

	static constexpr Wolf::ImageCompression::Compression DEFAULT_ALBEDO_COMPRESSION = Wolf::ImageCompression::Compression::BC1;
	static constexpr Wolf::ImageCompression::Compression DEFAULT_NORMAL_COMPRESSION = Wolf::ImageCompression::Compression::BC5;

	struct TextureSetFileInfoGGX
	{
		std::string name;

		std::string albedo;
		std::string normal;
		std::string roughness;
		std::string metalness;
		std::string ao;
		std::string anisoStrength;
	};
	TextureSetLoader(const TextureSetFileInfoGGX& textureSet, const OutputLayout& outputLayout, bool useCache, const Wolf::ResourceReference<AssetManager>& assetManager);

	struct TextureSetFileInfoSixWayLighting
	{
		std::string name;

		std::string tex0; // r = right, g = top, b = back, a = transparency
		std::string tex1; // r = left, g = bottom, b = front, a = unused (emissive ?)
	};
	TextureSetLoader(const TextureSetFileInfoSixWayLighting& textureSet, const Wolf::ResourceReference<AssetManager>& assetManager);

	struct TextureSetFileInfoAlphaOnly
	{
		std::string name;

		std::string alphaMap;
	};
	TextureSetLoader(const TextureSetFileInfoAlphaOnly& textureSet, const Wolf::ResourceReference<AssetManager>& assetManager);

	void transferImageTo(uint32_t idx, Wolf::ResourceUniqueOwner<Wolf::Image>& output);
	[[nodiscard]] const std::string& getOutputSlicesFolder(uint32_t idx) const { return m_outputFolders[idx]; }
	[[nodiscard]] AssetId getImageAssetId(uint32_t idx) const { return m_imageAssetIds[idx]; }

private:
	std::array<AssetId, Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL> m_imageAssetIds;

	std::array<Wolf::ResourceUniqueOwner<Wolf::Image>, Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL> m_outputImages;
	std::array<std::string, Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL> m_outputFolders;
	bool m_useCache;

	static Wolf::Format findFormatFromCompression(Wolf::ImageCompression::Compression compression, bool sRGB);

	// Load image full on GPU
	[[nodiscard]] bool createImageFileFromCache(const std::string& filename, bool sRGB, Wolf::ImageCompression::Compression compression, uint32_t imageIdx);
	template <typename PixelType>
	void createImageFileFromSource(const std::string& filename, bool sRGB, Wolf::ImageCompression::Compression compression, uint32_t imageIdx);

	// Use virtual texture
	template <typename PixelType, typename CompressionType>
	void createSlicedCacheFromData(const std::string& binFolder, Wolf::Extent3D extent, const std::vector<PixelType>& pixels, const std::vector<std::vector<PixelType>>& mipLevels);
	template <typename PixelType, typename CompressionType>
	void createSlicedCacheFromFile(const std::string& filename, bool sRGB, Wolf::ImageCompression::Compression compression, uint32_t imageIdx);

	void loadImageFile(const std::string& filename, Wolf::Format format, std::vector<Wolf::ImageCompression::RGBA8>& pixels, std::vector<std::vector<Wolf::ImageCompression::RGBA8>>& mipLevels, Wolf::Extent3D& outExtent) const;
	void loadImageFile(const std::string& filename, Wolf::Format format, std::vector<Wolf::ImageCompression::RG32F>& pixels, std::vector<std::vector<Wolf::ImageCompression::RG32F>>& mipLevels, Wolf::Extent3D& outExtent) const;
	void createImageFromData(Wolf::Extent3D extent, Wolf::Format format, const unsigned char* pixels, const std::vector<const unsigned char*>& mipLevels, uint32_t idx);

	template <typename CompressionType, typename PixelType>
	void compressAndCreateImage(std::vector<std::vector<PixelType>>& mipLevels, const std::vector<PixelType>& pixels, Wolf::Extent3D& extent, Wolf::Format format, const std::string& filename, std::fstream& outCacheFile,
		uint32_t imageIdx);

	// Combine roughness, metalness, ao and anisotrength into the same texture
	void createCombinedTexture(const TextureSetFileInfoGGX& textureSet);
};
