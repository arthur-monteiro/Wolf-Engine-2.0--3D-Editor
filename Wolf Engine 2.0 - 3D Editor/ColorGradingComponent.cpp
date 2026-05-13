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
}

bool ColorGradingComponent::updateLUTImage()
{
    if (m_lutImageAssetId == NO_ASSET)
    {
        m_renderingPipeline->getCompositionPass()->releaseInputLUT();
        return true;
    }

    AssetImageInterface::LoadingRequest loadingRequest{};
    loadingRequest.m_keepDataOnCPU = true;
    loadingRequest.m_canBeVirtualized = false;
    loadingRequest.m_format = LUT_IMAGE_FORMAT;
    loadingRequest.m_loadMips = false;

    m_assetManager->requestImageLoading(m_lutImageAssetId, loadingRequest, true);
    Wolf::ResourceNonOwner<Wolf::Image> image = m_assetManager->getImage(m_lutImageAssetId, LUT_IMAGE_FORMAT);

    m_renderingPipeline->getCompositionPass()->setInputLUT(image);

    return true;
}

void ColorGradingComponent::onLUTImageMapChanged()
{
    if (static_cast<std::string>(m_lutImageParam) == "")
    {
        m_lutImageAssetId = NO_ASSET;
    }
    else
    {
        AssetId assetId = m_assetManager->getAssetIdForPath(m_lutImageParam);
        if (!m_assetManager->isImage(assetId))
        {
            Wolf::Debug::sendWarning("Asset is not an image");
            m_lutImageParam = "";
        }

        m_lutImageAssetId = assetId;
    }

    updateLUTImage();
    notifySubscribers();
}
