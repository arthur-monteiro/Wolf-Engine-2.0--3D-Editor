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

    std::array<EditorParamInterface*, 1> m_editorParams =
    {
        &m_exposure
    };
};