#include "AssetImage.h"

#include "AssetManager.h"
#include "EditorConfiguration.h"
#include "ImageFormatter.h"

AssetImage::AssetImage(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId,
	const std::function<void(AssetId)>& updateResourceInUICallback, AssetId parentAssetId)
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

void AssetImage::loadImage(const LoadingRequest& loadingRequest)
{
	if (m_editor->getLoadingPath().empty())
	{
		Wolf::Debug::sendCriticalError("Loading path is empty");
	}

	KeepDataMode keepDataMode = loadingRequest.m_keepDataMode;
	if (m_thumbnailGenerationRequested && keepDataMode == KeepDataMode::ONLY_GPU)
		keepDataMode = KeepDataMode::CPU_AND_GPU;
	keepDataMode = computeNeededKeepDataMode(loadingRequest.m_format, keepDataMode);

	if (keepDataMode == KeepDataMode::DONT_KEEP)
		return;

	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_editor->getLoadingPath());

	ImageFormatter imageFormatter(m_editorPushDataToGPU, fullFilePath, loadingRequest.m_format, loadingRequest.m_canBeVirtualized, keepDataMode, loadingRequest.m_loadMips);
	if (keepDataMode != KeepDataMode::ONLY_CPU)
	{
		Wolf::ResourceUniqueOwner<Wolf::Image>& image = m_images[loadingRequest.m_format];

		imageFormatter.transferImageTo(image);
		m_slicesFolder = imageFormatter.getSlicesFolder();
	}

	if (keepDataMode != KeepDataMode::ONLY_GPU)
	{
		CPUData& cpuData = m_cpuData[loadingRequest.m_format];

		Wolf::Extent3D imageExtent = imageFormatter.getExtent();
		cpuData.m_extent = imageExtent;

		float imageBPP = Wolf::Image::computeBPPFromFormat(loadingRequest.m_format);

		cpuData.m_mipData.resize(imageFormatter.getMipCountKeptOnCPU() + 1);
		for (uint32_t mipLevel = 0; mipLevel < cpuData.m_mipData.size(); mipLevel++)
		{
			const Wolf::Extent3D mipExtent = { imageExtent.width >> mipLevel, imageExtent.height >> mipLevel, imageExtent.depth };
			cpuData.m_mipData[mipLevel].resize(imageBPP * mipExtent.width * mipExtent.height * mipExtent.depth);
			memcpy(cpuData.m_mipData[mipLevel].data(), imageFormatter.getPixels(mipLevel), cpuData.m_mipData[mipLevel].size());
		}

		if (m_thumbnailGenerationRequested)
		{
			std::string iconPath = AssetManager::computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);

			if (generateThumbnail(fullFilePath, iconPath))
			{
				m_updateAssetInUICallback(m_assetId);
			}
		}

		if (loadingRequest.m_keepDataMode == KeepDataMode::ONLY_GPU)
		{
			deleteImageData(loadingRequest.m_format);
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
	loadingRequest.m_canBeVirtualized = false;

	requestImageLoading(loadingRequest);
	m_thumbnailGenerationRequested = true;
}
