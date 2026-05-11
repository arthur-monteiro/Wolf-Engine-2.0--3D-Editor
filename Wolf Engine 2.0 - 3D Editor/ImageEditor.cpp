#include "ImageEditor.h"

#include "EditorParamsHelper.h"

ImageEditor::ImageEditor()
{

}

void ImageEditor::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_params);
}

void ImageEditor::activateParams()
{
    for (EditorParamInterface* editorParam : m_params)
    {
        editorParam->activate();
    }
}

void ImageEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
    for (const EditorParamInterface* editorParam : m_params)
    {
        editorParam->addToJSON(outJSON, tabCount, false);
    }
}

void ImageEditor::setLoadingPath(const std::string& path)
{
    m_filePathParam.setValueNoCallback(path);
}

void ImageEditor::onFilePathChanged()
{
    notifySubscribers();
}
