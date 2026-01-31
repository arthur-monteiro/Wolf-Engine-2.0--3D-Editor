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
#include "GPUDataTransfersManager.h"
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