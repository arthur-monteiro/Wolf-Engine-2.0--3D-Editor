#pragma once

#include <map>
#include <queue>

#include "AssetId.h"
#include "EditorGPUDataTransfersManager.h"

class ImageFormatter;

class AssetImageInterface
{
public:
    enum class KeepDataMode { DONT_KEEP, ONLY_GPU, CPU_AND_GPU, ONLY_CPU };

    AssetImageInterface() = delete;

    struct LoadingRequest
    {
        Wolf::Format m_format;
        bool m_loadMips;
        bool m_canBeVirtualized;
        KeepDataMode m_keepDataMode = KeepDataMode::ONLY_GPU;
    };
    void requestImageLoading(const LoadingRequest& loadingRequest);

    Wolf::ResourceNonOwner<Wolf::Image> getImage(Wolf::Format format);
    std::string getSlicesFolder() { return m_slicesFolder; }

    const uint8_t* getMipData(uint32_t mipLevel, Wolf::Format format) const;
    Wolf::Extent3D getExtent(Wolf::Format format) const;
    void deleteImageData(Wolf::Format format);
    void releaseImages();

protected:
    AssetImageInterface(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, bool needThumbnailsGeneration);

    bool generateThumbnail(const std::string& fullFilePath, const std::string& iconPath);
    KeepDataMode computeNeededKeepDataMode(Wolf::Format format, KeepDataMode requestedKeepDataMode) const;

    Wolf::ResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;

    std::queue<LoadingRequest> m_imageLoadingRequests;
    std::mutex m_loadingRequestsMutex;
    bool m_thumbnailGenerationRequested = false;

    std::map<Wolf::Format, Wolf::ResourceUniqueOwner<Wolf::Image>> m_images;
    std::string m_slicesFolder;

    struct CPUData
    {
        std::vector<std::vector<uint8_t>> m_mipData;
        Wolf::Extent3D m_extent;
    };
    std::map<Wolf::Format, CPUData> m_cpuData;
};
