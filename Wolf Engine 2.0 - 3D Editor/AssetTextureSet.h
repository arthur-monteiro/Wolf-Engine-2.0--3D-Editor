#pragma once

#include "AssetInterface.h"
#include "TextureSetEditor.h"

class AssetTextureSet  : public AssetInterface
{
public:
    AssetTextureSet(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
        const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, AssetManager* assetManager, AssetId parentAssetId);

    void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;

    bool isLoaded() const override;

    void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const override { outEditors.push_back(m_textureSetEditor.createNonOwnerResource<ComponentInterface>()); }
    Wolf::ResourceNonOwner<TextureSetEditor> getTextureSetEditor() const { return m_textureSetEditor.createNonOwnerResource(); }

private:
    void generateThumbnail();

    Wolf::ResourceUniqueOwner<TextureSetEditor> m_textureSetEditor;
    bool m_thumbnailGenerationRequested = false;
};
