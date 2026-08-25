#include "CameraSettingsComponent.h"

#include "EditorParamsHelper.h"
#include "ForwardPass.h"

CameraSettingsComponent::CameraSettingsComponent(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline) : m_renderingPipeline(renderingPipeline)
{
    m_exposure = 0.0f;
}

void CameraSettingsComponent::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_editorParams);
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

void CameraSettingsComponent::onCameraTypeChanged()
{
    if (m_cameraType == 0) // FPS
    {
        m_customCamera.reset(new Wolf::FirstPersonCamera(m_cameraPosition, m_cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f, m_cameraAspect));
    }
    else if (m_cameraType == 1) // Orthographic
    {
        m_customCamera.reset(new Wolf::OrthographicCamera(m_cameraPosition, m_cameraRadius, m_cameraHeightFromCenter,
            static_cast<glm::vec3>(m_cameraTarget) - static_cast<glm::vec3>(m_cameraPosition), 0.1f, 100.0f));
    }
}
