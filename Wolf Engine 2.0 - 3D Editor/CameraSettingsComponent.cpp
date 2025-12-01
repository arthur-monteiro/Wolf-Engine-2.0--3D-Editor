#include "CameraSettingsComponent.h"

#include "EditorParamsHelper.h"
#include "ForwardPass.h"

CameraSettingsComponent::CameraSettingsComponent(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline) : m_renderingPipeline(renderingPipeline)
{
    m_exposure = 0.0f;
}

void CameraSettingsComponent::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams(jsonReader, ID, m_editorParams);
}

void CameraSettingsComponent::activateParams()
{
    for (EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->activate();
    }
}

void CameraSettingsComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
    for (const EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->addToJSON(outJSON, tabCount, false);
    }
}

void CameraSettingsComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
    m_renderingPipeline->getForwardPass()->setExposure(m_exposure);
}
