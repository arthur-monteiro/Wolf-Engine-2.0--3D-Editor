#pragma once

#include "AssetInterface.h"
#include "ParticleEditor.h"

class AssetManager;

class AssetParticle  : public AssetInterface
{
public:
    AssetParticle(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
        const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, AssetManager* assetManager);

    void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass) override;

    bool isLoaded() const override;

    void getEditors(std::vector<Wolf::ResourceNonOwner<ComponentInterface>>& outEditors) const override { outEditors.push_back(m_particleEditor.createNonOwnerResource<ComponentInterface>()); }
    Wolf::ResourceNonOwner<ParticleEditor> getEditor() const { return m_particleEditor.createNonOwnerResource(); }

private:
    void generateThumbnail();

    Wolf::ResourceUniqueOwner<ParticleEditor> m_particleEditor;
    bool m_thumbnailGenerationRequested = false;
};