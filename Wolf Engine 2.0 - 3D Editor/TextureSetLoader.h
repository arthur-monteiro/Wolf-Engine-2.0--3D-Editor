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

	[[nodiscard]] const std::string& getOutputSlicesFolder(uint32_t idx) const { return m_outputFolders[idx]; }
	[[nodiscard]] AssetId getImageAssetId(uint32_t idx) const { return m_imageAssetIds[idx]; }

private:
	std::array<AssetId, Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL> m_imageAssetIds;
	std::array<std::string, Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL> m_outputFolders;
	bool m_useCache;
};
