#include "CombinedImageEditor.h"

#include "AssetManager.h"
#include "EditorParamsHelper.h"

CombinedImageEditor::CombinedImageEditor(AssetManager* assetManager) : m_assetManager(assetManager)
{

}

void CombinedImageEditor::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_params);
}

void CombinedImageEditor::activateParams()
{
    for (EditorParamInterface* editorParam : m_params)
    {
        editorParam->activate();
    }
}

void CombinedImageEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
    for (const EditorParamInterface* editorParam : m_params)
    {
        editorParam->addToJSON(outJSON, tabCount, false);
    }
}

void CombinedImageEditor::onFilePathChanged(EditorParamString& param, uint32_t channelIdx)
{
    if (static_cast<std::string>(param) == "")
        return;

    AssetId assetId = m_assetManager->getAssetIdForPath(param);
    if (!m_assetManager->isImage(assetId))
    {
        Wolf::Debug::sendWarning("Asset is not an image");
        param = "";
    }

    m_imageAssetIds[channelIdx] = assetId;
    notifySubscribers();
}