#include "AssetParticle.h"

AssetParticle::AssetParticle(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
    const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, AssetManager* assetManager)
: AssetInterface(loadingPath, assetId, updateResourceInUICallback, NO_ASSET)
{
    m_particleEditor.reset(new ParticleEditor(materialGPUManager, assetManager));
    m_particleEditor->subscribe(this, [this](Flags)
    {
        m_thumbnailGenerationRequested = true;
    });

    const std::ifstream inFile(g_editorConfiguration->computeFullPathFromLocalPath(loadingPath));
    if (!loadingPath.empty() && inFile.good())
    {
        Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { g_editorConfiguration->computeFullPathFromLocalPath(loadingPath) });
        m_particleEditor->loadParams(jsonReader);
    }

    m_thumbnailGenerationRequested = !g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration;
}

void AssetParticle::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
    const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
    m_particleEditor->updateBeforeFrame();
}

bool AssetParticle::isLoaded() const
{
    // TODO
    return true;
}

void AssetParticle::generateThumbnail()
{
    // TODO
}

