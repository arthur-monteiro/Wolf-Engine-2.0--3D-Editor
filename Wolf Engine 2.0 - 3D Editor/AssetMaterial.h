#pragma once

#include "AssetInterface.h"
#include "MaterialEditor.h"

class AssetManager;

class AssetMaterial : public AssetInterface
{
public:
    AssetMaterial(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
        const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, AssetManager* assetManager, AssetId parentAssetId);

    void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;

    bool isLoaded() const override;

    Wolf::ResourceNonOwner<MaterialEditor> getEditor() const { return m_materialEditor.createNonOwnerResource(); }
    void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const override { outEditors.push_back(getEditor().duplicateAs<ComponentInterface>()); }

private:
    void generateThumbnail();

    Wolf::ResourceUniqueOwner<MaterialEditor> m_materialEditor;
    bool m_thumbnailGenerationRequested = false;
};
