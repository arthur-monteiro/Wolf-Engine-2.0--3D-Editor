#pragma once

#include "AssetImageInterface.h"
#include "AssetInterface.h"
#include "CombinedImageEditor.h"

class AssetManager;

class AssetCombinedImage : public AssetInterface, public AssetImageInterface
{
public:
    AssetCombinedImage(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& loadingPath, bool needThumbnailsGeneration,
        AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateAssetInUICallback, AssetManager* assetManager, AssetId parentAssetId);

    void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;
    bool isLoaded() const override;

    void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const override { outEditors.push_back(m_editor.createNonOwnerResource<ComponentInterface>()); }

    Wolf::ResourceNonOwner<CombinedImageEditor> getEditor() const { return m_editor.createNonOwnerResource(); }

private:
    void loadImage(const LoadingRequest& loadingRequest);

    AssetManager* m_assetManager;
    Wolf::ResourceUniqueOwner<CombinedImageEditor> m_editor;
};
