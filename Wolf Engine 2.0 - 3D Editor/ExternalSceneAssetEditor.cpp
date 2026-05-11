#include "ExternalSceneAssetEditor.h"

#include "EditorParamsHelper.h"

ExternalSceneAssetEditor::ExternalSceneAssetEditor()
{
}

void ExternalSceneAssetEditor::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_params);
}

void ExternalSceneAssetEditor::activateParams()
{
    for (EditorParamInterface* editorParam : m_params)
    {
        editorParam->activate();
    }
}

void ExternalSceneAssetEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
    for (const EditorParamInterface* editorParam : m_params)
    {
        editorParam->addToJSON(outJSON, tabCount, false);
    }
}

void ExternalSceneAssetEditor::onFilePathChanged()
{
    notifySubscribers();
}
