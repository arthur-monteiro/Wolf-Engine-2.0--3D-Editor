#include "AssetTextureSet.h"

#include <stb_image_write.h>

#include "AssetManager.h"
#include "EditorConfiguration.h"

#include "ImageFileLoader.h"

AssetTextureSet::AssetTextureSet(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
    const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, AssetManager* assetManager, AssetId parentAssetId)
: AssetInterface(loadingPath, assetId, updateResourceInUICallback, parentAssetId)
{
    m_textureSetEditor.reset(new TextureSetEditor(materialGPUManager, assetManager, assetId));
    m_textureSetEditor->subscribe(this, [this](Flags)
    {
        m_thumbnailGenerationRequested = true;
    });

    const std::ifstream inFile(g_editorConfiguration->computeFullPathFromLocalPath(loadingPath));
    if (!loadingPath.empty() && inFile.good())
    {
        Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { g_editorConfiguration->computeFullPathFromLocalPath(loadingPath) });
        m_textureSetEditor->loadParams(jsonReader);
    }

    m_thumbnailGenerationRequested = !g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration;
}

void AssetTextureSet::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
    m_textureSetEditor->updateBeforeFrame();

    if (m_thumbnailGenerationRequested)
    {
        generateThumbnail();
        m_thumbnailGenerationRequested = false;
    }
}

bool AssetTextureSet::isLoaded() const
{
    return m_textureSetEditor->getTextureSetIdx() != 0;
}

void AssetTextureSet::generateThumbnail()
{
    std::string iconPath = AssetManager::computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);
    if (std::filesystem::exists(iconPath))
    {
        std::error_code ec;
        if (!std::filesystem::remove(iconPath, ec))
        {
            // Thumbnail file exists but is OS locked, we need to create a new file with a new name
            m_thumbnailCountToMaintain++;
            iconPath = AssetManager::computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);
        }
    }

    constexpr int ICON_SIZE = ThumbnailsGenerationPass::OUTPUT_SIZE;
    constexpr int SLICE_WIDTH = ICON_SIZE / 5;
    constexpr int CHANNEL_COUNT = 4;

    Wolf::ImageFileLoader albedoLoader(AssetManager::computeIconPath(m_textureSetEditor->getAlbedoPath(), 0));
    Wolf::ImageFileLoader normalLoader(AssetManager::computeIconPath(m_textureSetEditor->getNormalPath(), 0));
    Wolf::ImageFileLoader roughnessLoader(AssetManager::computeIconPath(m_textureSetEditor->getRoughnessPath(), 0));
    Wolf::ImageFileLoader metalnessLoader(AssetManager::computeIconPath(m_textureSetEditor->getMetalnessPath(), 0));
    Wolf::ImageFileLoader aoLoader(AssetManager::computeIconPath(m_textureSetEditor->getAOPath(), 0));

    unsigned char* loaders[] = {
        albedoLoader.getPixels(),
        normalLoader.getPixels(),
        roughnessLoader.getPixels(),
        metalnessLoader.getPixels(),
        aoLoader.getPixels()
    };

    std::vector<unsigned char> combinedPixels(ICON_SIZE * ICON_SIZE * CHANNEL_COUNT);

    for (uint32_t y = 0; y < ICON_SIZE; ++y)
    {
        for (uint32_t x = 0; x < ICON_SIZE; ++x)
        {
            uint32_t sliceIndex = x / SLICE_WIDTH;
            if (sliceIndex > 4) sliceIndex = 4;

            unsigned char* sourcePixels = loaders[sliceIndex];

            uint32_t pixelOffset = (y * ICON_SIZE + x) * CHANNEL_COUNT;

            if (sourcePixels)
            {
                combinedPixels[pixelOffset + 0] = sourcePixels[pixelOffset + 0];
                combinedPixels[pixelOffset + 1] = sourcePixels[pixelOffset + 1];
                combinedPixels[pixelOffset + 2] = sourcePixels[pixelOffset + 2];
                combinedPixels[pixelOffset + 3] = sourcePixels[pixelOffset + 3];
            }
        }
    }

    stbi_write_png(iconPath.c_str(), ICON_SIZE, ICON_SIZE, CHANNEL_COUNT, combinedPixels.data(), ICON_SIZE * CHANNEL_COUNT);

    m_updateAssetInUICallback(computeName(), iconPath.substr(3, iconPath.size()), m_assetId);
}
