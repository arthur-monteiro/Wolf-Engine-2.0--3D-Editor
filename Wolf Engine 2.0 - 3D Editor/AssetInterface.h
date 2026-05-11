#pragma once

#include <string>

#include <MaterialsGPUManager.h>

#include "AssetId.h"
#include "Notifier.h"
#include "ThumbnailsGenerationPass.h"

class ComponentInterface;

class AssetInterface : public Notifier
{
public:
    AssetInterface(const std::string& loadingPath, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
        AssetId parentAssetId);
    AssetInterface(const AssetInterface&) = delete;
    virtual ~AssetInterface() = default;

    virtual void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) = 0;

    virtual bool isLoaded() const = 0;
    std::string getLoadingPath() const { return m_loadingPath; }
    std::string computeName() const;

    void onChanged() const;

    virtual void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const = 0;
    AssetId getParentAssetId() const { return m_parentAssetId; }

protected:
    std::string m_loadingPath;
    AssetId m_assetId = NO_ASSET;
    AssetId m_parentAssetId = NO_ASSET;
    std::function<void(const std::string&, const std::string&, AssetId)> m_updateAssetInUICallback;

    // Because the thumbnail file is locked by the UI, when we want to update it we need to create a new file with a new name
    // This counter indicates the name of the next file: (icon name)_(m_thumbnailCountToMaintain)_.(extension)
    uint32_t m_thumbnailCountToMaintain = 0;
};
