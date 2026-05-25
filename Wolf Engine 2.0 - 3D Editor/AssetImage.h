#pragma once

#include "AssetImageInterface.h"
#include "AssetInterface.h"
#include "ImageEditor.h"

class AssetImage : public AssetInterface, public AssetImageInterface
{
public:
    AssetImage(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId,
        const std::function<void(AssetId)>& updateResourceInUICallback, AssetId parentAssetId);
    AssetImage(const AssetImage&) = delete;

    void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
    bool isLoaded() const override;

    void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const override { outEditors.push_back(m_editor.createNonOwnerResource<ComponentInterface>()); }

    Wolf::ResourceNonOwner<ImageEditor> getEditor() const { return m_editor.createNonOwnerResource(); }

private:
    void loadImage(const LoadingRequest& loadingRequest);
    bool m_preventThumbnailsGeneration = true;
    void recomputeThumbnail();

    Wolf::ResourceUniqueOwner<ImageEditor> m_editor;
};
