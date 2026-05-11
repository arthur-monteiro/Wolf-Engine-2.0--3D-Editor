#include "AssetInterface.h"

#include "EditorConfiguration.h"

AssetInterface::AssetInterface(const std::string& loadingPath, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback, AssetId parentAssetId)
    : m_loadingPath(EditorConfiguration::sanitizeFilePath(loadingPath)), m_assetId(assetId), m_updateAssetInUICallback(updateResourceInUICallback), m_parentAssetId(parentAssetId)
{
}

std::string AssetInterface::computeName() const
{
    std::filesystem::path loadingPath(g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath));
    std::filesystem::path filename = loadingPath.filename();

    return filename.string();
}

void AssetInterface::onChanged() const
{
    notifySubscribers();
}
