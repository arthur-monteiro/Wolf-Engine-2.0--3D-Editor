#include "AssetMaterial.h"

#include <JSONReader.h>

#include "EditorConfiguration.h"

AssetMaterial::AssetMaterial(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
                           const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, AssetManager* assetManager, AssetId parentAssetId)
: AssetInterface(loadingPath, assetId, updateResourceInUICallback, parentAssetId)
{
    m_materialEditor.reset(new MaterialEditor(materialGPUManager, assetManager));

    const std::ifstream inFile(g_editorConfiguration->computeFullPathFromLocalPath(loadingPath));
    if (!loadingPath.empty() && inFile.good())
    {
        Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { g_editorConfiguration->computeFullPathFromLocalPath(loadingPath) });
        m_materialEditor->loadParams(jsonReader);
    }

    m_thumbnailGenerationRequested = !g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration;
}

void AssetMaterial::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
    m_materialEditor->updateBeforeFrame();

    if (m_thumbnailGenerationRequested)
    {
        generateThumbnail();
        m_thumbnailGenerationRequested = false;
    }
}

bool AssetMaterial::isLoaded() const
{
    return m_materialEditor->getMaterialGPUIdx() != 0;
}

void AssetMaterial::generateThumbnail()
{
    // TODO
}

