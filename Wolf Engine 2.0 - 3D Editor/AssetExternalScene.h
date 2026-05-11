#pragma once

#include "AssetInterface.h"
#include "ExternalSceneAssetEditor.h"
#include "ExternalSceneLoader.h"

class AssetManager;

class AssetExternalScene : public AssetInterface
{
public:
    AssetExternalScene(AssetManager* assetManager, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId,
        const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback, AssetId parentAssetId);
    AssetExternalScene(const AssetExternalScene&) = delete;
    ~AssetExternalScene() override = default;

    void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;

    bool isLoaded() const override;

    void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const override { outEditors.push_back(m_editor.createNonOwnerResource<ComponentInterface>()); }

    const Wolf::AABB& getAABB();
    const std::vector<AssetId>& getModelAssetIds() const { return m_meshAssetIds; }
    const std::vector<ExternalSceneLoader::InstanceData>& getInstances() const { return m_instances; }

    Wolf::ResourceNonOwner<ExternalSceneAssetEditor> getEditor() const { return m_editor.createNonOwnerResource(); }

private:
    void loadScene(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager);
    void generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass);

    Wolf::ResourceUniqueOwner<ExternalSceneAssetEditor> m_editor;

    AssetManager* m_assetManager = nullptr;
    bool m_loadingRequested = false;
    bool m_thumbnailGenerationRequested = false;

    std::vector<AssetId> m_meshAssetIds;
    Wolf::AABB m_aabb;

    std::vector<ExternalSceneLoader::InstanceData> m_instances;
};
