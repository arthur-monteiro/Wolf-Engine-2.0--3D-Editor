#pragma once

#include <map>
#include <queue>

#include "AssetId.h"
#include "EditorGPUDataTransfersManager.h"

class ImageFormatter;

class AssetImageInterface
{
public:
    AssetImageInterface() = delete;

    struct LoadingRequest
    {
        Wolf::Format m_format;
        bool m_loadMips;
        bool m_canBeVirtualized;
        bool m_keepDataOnCPU;
    };
    void requestImageLoading(const LoadingRequest& loadingRequest);

    Wolf::ResourceNonOwner<Wolf::Image> getImage(Wolf::Format format);
    std::string getSlicesFolder() { return m_slicesFolder; }

    void deleteImageData();
    void releaseImages();

protected:
    AssetImageInterface(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, bool needThumbnailsGeneration);

    bool generateThumbnail(const std::string& fullFilePath, const std::string& iconPath);

    Wolf::ResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;

    std::queue<LoadingRequest> m_imageLoadingRequests;
    std::mutex m_loadingRequestsMutex;
    bool m_thumbnailGenerationRequested = false;

    std::map<Wolf::Format, Wolf::ResourceUniqueOwner<Wolf::Image>> m_images;
    std::string m_slicesFolder;

    enum class DataOnCPUStatus { NEVER_KEPT, NOT_LOADED_YET, AVAILABLE, DELETED } m_dataOnCPUStatus = DataOnCPUStatus::NOT_LOADED_YET;
    std::vector<std::vector<uint8_t>> m_mipData;
    Wolf::Format m_mipDataFormat;
};
