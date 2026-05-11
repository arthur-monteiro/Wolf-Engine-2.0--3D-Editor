#include "TextureSetLoader.h"

#include <filesystem>

#include "AssetManager.h"
#include "ImageFileLoader.h"
#include "Timer.h"

TextureSetLoader::TextureSetLoader(const TextureSetAssetsInfoGGX& textureSet, const OutputLayout& outputLayout, bool useCache, const Wolf::ResourceReference<AssetManager>& assetManager, AssetId parentAssetId) : m_useCache(useCache)
{
	Wolf::Debug::sendInfo("Loading texture set " + textureSet.m_name);
	Wolf::Timer globalTimer("Loading texture set " + textureSet.m_name);

	// Albedo
	m_imageAssetIds[0] = textureSet.m_albedoAssetId;
	if (textureSet.m_albedoAssetId != NO_ASSET)
	{
		if (outputLayout.albedoCompression != DEFAULT_ALBEDO_COMPRESSION)
		{
			Wolf::Debug::sendCriticalError("Albedo compression must be BC1");
		}


		AssetImageInterface::LoadingRequest loadingRequest{};
		loadingRequest.m_format = Wolf::Format::BC1_RGB_SRGB_BLOCK;
		loadingRequest.m_loadMips = true;
		loadingRequest.m_canBeVirtualized = true;
		assetManager->requestImageLoading(textureSet.m_albedoAssetId, loadingRequest, true);
	}

	// Normal
	m_imageAssetIds[1] = textureSet.m_normalAssetId;
	if (textureSet.m_normalAssetId != NO_ASSET)
	{
		if (outputLayout.normalCompression != DEFAULT_NORMAL_COMPRESSION)
		{
			Wolf::Debug::sendError("Normal compression must be BC5");
		}

		AssetImageInterface::LoadingRequest loadingRequest{};
		loadingRequest.m_format = Wolf::Format::BC5_UNORM_BLOCK;
		loadingRequest.m_loadMips = true;
		loadingRequest.m_canBeVirtualized = true;
		assetManager->requestImageLoading(textureSet.m_normalAssetId, loadingRequest, true);
	}


	if (textureSet.m_roughnessAssetId != NO_ASSET)
	{
		Wolf::Debug::sendInfo("Loading combined image");

		std::array<AssetId, 4> channelAssetsId = { textureSet.m_roughnessAssetId, textureSet.m_metalnessAssetId, textureSet.m_aoAssetId, textureSet.m_anisoStrengthAssetId };

		std::string folderPath;
		std::array<std::string, 4> channelFilenames;
		for (uint32_t channelIdx = 0; channelIdx < 4; channelIdx++)
		{
			if (channelAssetsId[channelIdx] == NO_ASSET)
			{
				channelFilenames[channelIdx] = "noAsset";
				continue;
			}

			std::string loadingPathStr = g_editorConfiguration->computeFullPathFromLocalPath(assetManager->getImageLoadingPath(channelAssetsId[channelIdx]));
			std::filesystem::path loadingPath(loadingPathStr);

			if (channelIdx == 0)
			{
				folderPath = loadingPath.parent_path().string();
			}
			channelFilenames[channelIdx] = loadingPath.filename().string();
		}

		std::string combinedLoadingPath = folderPath + "/" + "combined_";
		for (uint32_t channelIdx = 0; channelIdx < 4; channelIdx++)
		{
			combinedLoadingPath += channelFilenames[channelIdx] + "_";
		}

		m_imageAssetIds[2] = assetManager->addCombinedImage(g_editorConfiguration->computeLocalPathFromFullPath(combinedLoadingPath), parentAssetId);
		Wolf::ResourceNonOwner<CombinedImageEditor> combinedImageEditor = assetManager->getCombinedImageEditor(m_imageAssetIds[2]);
		for (uint32_t channelIdx = 0; channelIdx < 4; ++channelIdx)
		{
			combinedImageEditor->setAssetId(channelIdx, channelAssetsId[channelIdx]);
		}

		AssetImageInterface::LoadingRequest loadingRequest{};
		loadingRequest.m_format = Wolf::Format::BC3_UNORM_BLOCK;
		loadingRequest.m_loadMips = true;
		loadingRequest.m_canBeVirtualized = true;
		assetManager->requestCombinedImageLoading(m_imageAssetIds[2], loadingRequest, true);
	}
	else
	{
		m_imageAssetIds[2] = NO_ASSET;
	}
}

TextureSetLoader::TextureSetLoader(const TextureSetAssetsInfoSixWayLighting& textureSet, const Wolf::ResourceReference<AssetManager>& assetManager)
{
	m_imageAssetIds[0] = textureSet.m_tex0AssetId;
	if (textureSet.m_tex0AssetId != NO_ASSET)
	{
		AssetImageInterface::LoadingRequest loadingRequest{};
		loadingRequest.m_format = Wolf::Format::R8G8B8A8_UNORM;
		loadingRequest.m_loadMips = true;
		loadingRequest.m_canBeVirtualized = true;
		assetManager->requestImageLoading(textureSet.m_tex0AssetId, loadingRequest, true);
	}

	m_imageAssetIds[1] = textureSet.m_tex1AssetId;
	if (textureSet.m_tex1AssetId != NO_ASSET)
	{
		AssetImageInterface::LoadingRequest loadingRequest{};
		loadingRequest.m_format = Wolf::Format::R8G8B8A8_UNORM;
		loadingRequest.m_loadMips = true;
		loadingRequest.m_canBeVirtualized = true;
		assetManager->requestImageLoading(textureSet.m_tex1AssetId, loadingRequest, true);
	}
}

TextureSetLoader::TextureSetLoader(const TextureSetAssetInfoAlphaOnly& textureSet, const Wolf::ResourceReference<AssetManager>& assetManager)
{
	m_imageAssetIds[0] = textureSet.m_alphaMapAssetId;
	if (textureSet.m_alphaMapAssetId != NO_ASSET)
	{
		AssetImageInterface::LoadingRequest loadingRequest{};
		loadingRequest.m_format = Wolf::Format::R8G8B8A8_UNORM;
		loadingRequest.m_loadMips = true;
		loadingRequest.m_canBeVirtualized = true;
		assetManager->requestImageLoading(textureSet.m_alphaMapAssetId, loadingRequest, true);
	}
}