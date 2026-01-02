#include "ColorGradingComponent.h"

#include "CompositionPass.h"
#include "EditorParamsHelper.h"

ColorGradingComponent::ColorGradingComponent(const Wolf::ResourceNonOwner<AssetManager>& resourceManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
    : m_resourceManager(resourceManager), m_renderingPipeline(renderingPipeline)
{
}

void ColorGradingComponent::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams(jsonReader, ID, m_editorParams);
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
    if (static_cast<std::string>(m_lutImageParam) == "")
    {
        m_renderingPipeline->getCompositionPass()->releaseInputLUT();
        return true;
    }

    if (m_lutImageResourceId == NO_ASSET || !m_resourceManager->isImageLoaded(m_lutImageResourceId))
        return false;

    Wolf::ResourceNonOwner<Wolf::Image> image = m_resourceManager->getImage(m_lutImageResourceId);

    m_renderingPipeline->getCompositionPass()->setInputLUT(image);

    return true;
}

void ColorGradingComponent::onLUTImageMapChanged()
{
    if (static_cast<std::string>(m_lutImageParam) != "")
    {
        m_lutImageResourceId = m_resourceManager->addImage(m_lutImageParam, false, Wolf::Format::R16G16B16A16_SFLOAT, false, false);
    }
    else
    {
        m_lutImageResourceId = NO_ASSET;
    }
    m_lutImageUpdateRequested = true;
}
