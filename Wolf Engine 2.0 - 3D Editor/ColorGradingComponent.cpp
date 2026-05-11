#include "ColorGradingComponent.h"

#include "CompositionPass.h"
#include "EditorParamsHelper.h"

ColorGradingComponent::ColorGradingComponent(const Wolf::ResourceNonOwner<AssetManager>& resourceManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
    : m_assetManager(resourceManager), m_renderingPipeline(renderingPipeline)
{
}

void ColorGradingComponent::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_editorParams);
}

void ColorGradingComponent::activateParams()
{
    for (EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->activate();
    }
}

void ColorGradingComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
    for (const EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->addToJSON(outJSON, tabCount, false);
    }
}

void ColorGradingComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
    if (m_lutImageUpdateRequested)
    {
        if (updateLUTImage())
        {
            m_lutImageUpdateRequested = false;
        }
    }
}

bool ColorGradingComponent::updateLUTImage()
{
    // TODO
    return true;
}

void ColorGradingComponent::onLUTImageMapChanged()
{
    if (static_cast<std::string>(m_lutImageParam) != "")
    {
        m_lutImageResourceId = m_assetManager->addImage(m_lutImageParam);
    }
    else
    {
        m_lutImageResourceId = NO_ASSET;
    }
    m_lutImageUpdateRequested = true;
}
