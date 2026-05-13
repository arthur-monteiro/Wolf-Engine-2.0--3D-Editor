#include "AssetImage.h"

#include "AssetManager.h"
#include "EditorConfiguration.h"
#include "ImageFormatter.h"

AssetImage::AssetImage(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId,
	const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback, AssetId parentAssetId)
	: AssetInterface(loadingPath, assetId, updateResourceInUICallback, parentAssetId), AssetImageInterface(editorPushDataToGPU, needThumbnailsGeneration)
{
	m_editor.reset(new ImageEditor());
	m_editor->subscribe(this, [this](Flags)
	{
		recomputeThumbnail();
	});

	const std::ifstream inFile(g_editorConfiguration->computeFullPathFromLocalPath(loadingPath));
	if (!loadingPath.empty() && inFile.good())
	{
		Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { g_editorConfiguration->computeFullPathFromLocalPath(loadingPath) });
		m_editor->loadParams(jsonReader);
	}

	m_preventThumbnailsGeneration = false;
}

void AssetImage::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	m_loadingRequestsMutex.lock();
	while (!m_imageLoadingRequests.empty())
	{
		loadImage(m_imageLoadingRequests.front());
		m_imageLoadingRequests.pop();
	}
	m_loadingRequestsMutex.unlock();
}

bool AssetImage::isLoaded() const
{
	return !m_images.empty();
}

const uint8_t* AssetImage::getMipData(uint32_t mipLevel, Wolf::Format format) const
{
	if (m_dataOnCPUStatus != DataOnCPUStatus::AVAILABLE)
	{
		Wolf::Debug::sendError("Data requested in not here");
		return nullptr;
	}

	if (m_mipDataFormat != format)
	{
		Wolf::Debug::sendCriticalError("Data is available but with a different format");
	}

	return m_mipData[mipLevel].data();
}

void AssetImage::loadImage(const LoadingRequest& loadingRequest)
{
	if (m_editor->getLoadingPath().empty())
	{
		Wolf::Debug::sendCriticalError("Loading path is empty");
	}

	if (m_images.contains(loadingRequest.m_format))
	{
		return;
	}

	m_images[loadingRequest.m_format].reset(nullptr);
	Wolf::ResourceUniqueOwner<Wolf::Image>& image = m_images[loadingRequest.m_format];

	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_editor->getLoadingPath());

	bool keepDataOnCPU = m_thumbnailGenerationRequested || loadingRequest.m_keepDataOnCPU;
	ImageFormatter imageFormatter(m_editorPushDataToGPU, fullFilePath, loadingRequest.m_format, loadingRequest.m_canBeVirtualized,
		keepDataOnCPU ? ImageFormatter::KeepDataMode::CPU_AND_GPU : ImageFormatter::KeepDataMode::ONLY_GPU, loadingRequest.m_loadMips);
	imageFormatter.transferImageTo(image);
	m_slicesFolder = imageFormatter.getSlicesFolder();

	if (!image)
		return;

	if (keepDataOnCPU)
	{
		m_mipData.resize(imageFormatter.getMipCountKeptOnCPU() + 1);
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

void AssetImage::recomputeThumbnail()
{
	if (m_preventThumbnailsGeneration)
		return;

	LoadingRequest loadingRequest{};

	std::string fileExtension = m_editor->getLoadingPath().substr(m_editor->getLoadingPath().find_last_of(".") + 1);
	loadingRequest.m_format = Wolf::Format::R8G8B8A8_UNORM;
	if (fileExtension == "cube")
	{
		loadingRequest.m_format = Wolf::Format::R16G16B16A16_SFLOAT;
	}
	else if (fileExtension == "hdr")
	{
		loadingRequest.m_format = Wolf::Format::R32G32B32A32_SFLOAT;
	}
	loadingRequest.m_loadMips = fileExtension == "cube" ? false : true;
	loadingRequest.m_canBeVirtualized = true;

	requestImageLoading(loadingRequest);
	m_thumbnailGenerationRequested = true;
}
