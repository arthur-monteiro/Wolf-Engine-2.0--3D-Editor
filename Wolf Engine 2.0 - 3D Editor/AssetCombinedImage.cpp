#include "AssetCombinedImage.h"

#include "AssetManager.h"
#include "EditorConfiguration.h"
#include "ImageFormatter.h"

AssetCombinedImage::AssetCombinedImage(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& loadingPath, bool needThumbnailsGeneration,
	AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateAssetInUICallback, AssetManager* assetManager, AssetId parentAssetId)
 : m_assetManager(assetManager), AssetInterface(loadingPath, assetId, updateAssetInUICallback, parentAssetId), AssetImageInterface(editorPushDataToGPU, needThumbnailsGeneration)
{
	m_editor.reset(new CombinedImageEditor(assetManager));
}

void AssetCombinedImage::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	m_loadingRequestsMutex.lock();
	while (!m_imageLoadingRequests.empty())
	{
		loadImage(m_imageLoadingRequests.front());
		m_imageLoadingRequests.pop();
	}
	m_loadingRequestsMutex.unlock();
}

bool AssetCombinedImage::isLoaded() const
{
	return !m_images.empty();
}

void AssetCombinedImage::loadImage(const LoadingRequest& loadingRequest)
{
	if (m_images.contains(loadingRequest.m_format))
	{
		return;
	}

	m_images[loadingRequest.m_format].reset(nullptr);
	Wolf::ResourceUniqueOwner<Wolf::Image>& image = m_images[loadingRequest.m_format];

	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath);

	std::string loadingPathR = m_editor->getLoadingPathR();
	std::string loadingPathG = m_editor->getLoadingPathG();
	std::string loadingPathB = m_editor->getLoadingPathB();
	std::string loadingPathA = m_editor->getLoadingPathA();

	std::string filenameNoExtensionR = EditorConfiguration::sanitizeFilePath(loadingPathR.substr(loadingPathR.find_last_of("/") + 1, loadingPathR.find_last_of(".")));
	std::string filenameNoExtensionG = EditorConfiguration::sanitizeFilePath(loadingPathG.substr(loadingPathG.find_last_of("/") + 1, loadingPathG.find_last_of(".")));
	std::string filenameNoExtensionB = EditorConfiguration::sanitizeFilePath(loadingPathB.substr(loadingPathB.find_last_of("/") + 1, loadingPathB.find_last_of(".")));
	std::string filenameNoExtensionA = EditorConfiguration::sanitizeFilePath(loadingPathA.substr(loadingPathA.find_last_of("/") + 1, loadingPathA.find_last_of(".")));
	std::string folder = loadingPathR.substr(0, loadingPathR.find_last_of("/"));

	bool keepDataOnCPU = m_thumbnailGenerationRequested || loadingRequest.m_keepDataOnCPU;

	if (!keepDataOnCPU && ImageFormatter::isCacheAvailable(fullFilePath, loadingRequest.m_format, loadingRequest.m_canBeVirtualized))
	{
		ImageFormatter imageFormatter(m_editorPushDataToGPU, fullFilePath, loadingRequest.m_format, loadingRequest.m_canBeVirtualized, ImageFormatter::KeepDataMode::ONLY_GPU,
			loadingRequest.m_loadMips);
		imageFormatter.transferImageTo(image);
		m_slicesFolder = imageFormatter.getSlicesFolder();
		return;
	}

	std::vector<Wolf::ImageCompression::RGBA8> combinedData;
	std::vector<std::vector<Wolf::ImageCompression::RGBA8>> combinedMipLevels;
	Wolf::Extent3D combinedExtent = { 1, 1, 1};
	combinedData.resize(1);

	for (uint32_t channelIdx = 0; channelIdx < 4; channelIdx++)
	{
		AssetId channelAssetId = m_editor->getAssetId(channelIdx);
		if (channelAssetId == NO_ASSET)
			continue;

		AssetImageInterface::LoadingRequest imageLoadingRequest{};
		imageLoadingRequest.m_format = Wolf::Format::R8G8B8A8_UNORM;
		imageLoadingRequest.m_loadMips = true;
		imageLoadingRequest.m_canBeVirtualized = false;
		imageLoadingRequest.m_keepDataOnCPU = true;
		m_assetManager->requestImageLoading(channelAssetId, imageLoadingRequest, true);
		Wolf::ResourceNonOwner<Wolf::Image> image = m_assetManager->getImage(channelAssetId, Wolf::Format::R8G8B8A8_UNORM);

		const uint8_t* pixels = m_assetManager->getImageData(channelAssetId, 0, Wolf::Format::R8G8B8A8_UNORM);
		std::vector<const uint8_t*> mipLevels(image->getMipLevelCount() - 1);
		for (uint32_t mipIdx = 0; mipIdx < mipLevels.size(); mipIdx++)
		{
			mipLevels[mipIdx] = m_assetManager->getImageData(channelAssetId, mipIdx + 1, Wolf::Format::R8G8B8A8_UNORM);
		}
		Wolf::Extent3D extent = image->getExtent();
		uint32_t pixelCount = extent.width * extent.height * extent.depth;

		if (channelIdx == 0)
		{
			combinedData.resize(pixelCount);
			combinedExtent = extent;
		}
		if (extent != combinedExtent)
		{
			Wolf::Debug::sendCriticalError("Extent of all channels must be the same");
		}
		const Wolf::ImageCompression::RGBA8* pixelsAsRGBA = reinterpret_cast<const Wolf::ImageCompression::RGBA8*>(pixels);
		for (uint32_t pixelIdx = 0; pixelIdx < pixelCount; ++pixelIdx)
		{
			combinedData[pixelIdx][channelIdx] = pixelsAsRGBA[pixelIdx][channelIdx];
		}

		if (channelIdx == 0)
		{
			combinedMipLevels.resize(mipLevels.size());
		}
		if (combinedMipLevels.size() != mipLevels.size())
		{
			Wolf::Debug::sendCriticalError("MipLevels of all channels must be the same");
		}
		for (uint32_t mipLevel = 0; mipLevel < mipLevels.size(); ++mipLevel)
		{
			const Wolf::ImageCompression::RGBA8* mipAsRGBA = reinterpret_cast<const Wolf::ImageCompression::RGBA8*>(mipLevels[mipLevel]);
			uint32_t mipPixelCount = (extent.width >> (mipLevel + 1)) * (extent.height >> (mipLevel + 1)) * extent.depth;

			if (channelIdx == 0)
			{
				combinedMipLevels[mipLevel].resize(mipPixelCount);
			}

			for (uint32_t pixelIdx = 0; pixelIdx < mipPixelCount; ++pixelIdx)
			{
				combinedMipLevels[mipLevel][pixelIdx][channelIdx] = mipAsRGBA[pixelIdx].r;
			}
		}
	}

	ImageFormatter imageFormatter(m_editorPushDataToGPU, combinedData, combinedMipLevels, combinedExtent, fullFilePath, loadingRequest.m_format, loadingRequest.m_canBeVirtualized,
		keepDataOnCPU ? ImageFormatter::KeepDataMode::CPU_AND_GPU : ImageFormatter::KeepDataMode::ONLY_GPU);
	imageFormatter.transferImageTo(image);
	m_slicesFolder = imageFormatter.getSlicesFolder();

	if (keepDataOnCPU)
	{
		m_mipData.resize(imageFormatter.getMipCountKeptOnCPU());
		for (uint32_t mipLevel = 0; mipLevel < m_mipData.size(); mipLevel++)
		{
			const Wolf::Extent3D mipExtent = { image->getExtent().width >> mipLevel, image->getExtent().height >> mipLevel, image->getExtent().depth };
			m_mipData[mipLevel].resize(image->getBPP() * mipExtent.width * mipExtent.height * mipExtent.depth);
			memcpy(m_mipData[mipLevel].data(), imageFormatter.getPixels(mipLevel), m_mipData[mipLevel].size());
		}
		m_mipDataFormat = loadingRequest.m_format;

		if (m_dataOnCPUStatus == DataOnCPUStatus::NOT_LOADED_YET)
		{
			m_dataOnCPUStatus = DataOnCPUStatus::AVAILABLE;
		}
	}

	if (m_thumbnailGenerationRequested)
	{
		std::string iconPath = AssetManager::computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);

		if (generateThumbnail(fullFilePath, iconPath))
		{
			m_updateAssetInUICallback(computeName(), iconPath.substr(3, iconPath.size()), m_assetId);
		}

		if (m_dataOnCPUStatus != DataOnCPUStatus::AVAILABLE)
		{
			deleteImageData();
		}
	}
}