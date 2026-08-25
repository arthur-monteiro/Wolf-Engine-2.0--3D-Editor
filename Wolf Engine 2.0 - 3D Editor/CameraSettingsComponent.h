#pragma once

#include "ComponentInterface.h"
#include "EditorTypes.h"

class CameraSettingsComponent : public ComponentInterface
{
public:
    static inline std::string ID = "cameraSettingsComponent";
    std::string getId() const override { return ID; }

    CameraSettingsComponent(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline);

    void loadParams(Wolf::JSONReader& jsonReader) override;
    void activateParams() override;
    void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

    void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
    void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
    void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

    void saveCustom() const override {}

private:
    inline static const std::string TAB = "Camera Settings";
    Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;

    EditorParamFloat m_exposure = EditorParamFloat("Exposure", TAB, "Graphics", -10.0f, 10.0f);

    EditorParamBool m_overrideCamera = EditorParamBool("Override camera", TAB, "Camera", false);

    // Camera overriden
    void onCameraTypeChanged();
    EditorParamEnum m_cameraType = EditorParamEnum({ "FPS", "Orthographic" }, "Type", TAB, "Camera", [this]() { onCameraTypeChanged(); });
    EditorParamVector3 m_cameraPosition = EditorParamVector3("Position", TAB, "Camera", -10.0f, 10.0f);
    EditorParamVector3 m_cameraTarget = EditorParamVector3("Target", TAB, "Camera", -10.0f, 10.0f);

    // FPS camera
    EditorParamFloat m_cameraAspect = EditorParamFloat("Aspect", TAB, "Camera", 0.0f, 2.0f);

    // Orthographic camera
    EditorParamFloat m_cameraRadius = EditorParamFloat("Radius", TAB, "Camera", 0.0f, 10.0f);
    EditorParamFloat m_cameraHeightFromCenter = EditorParamFloat("Height from center", TAB, "Camera", -10.0f, 10.0f);

    std::array<EditorParamInterface*, 1> m_editorParams =
    {
        &m_exposure
    };

    Wolf::ResourceUniqueOwner<Wolf::CameraInterface> m_customCamera;
};
