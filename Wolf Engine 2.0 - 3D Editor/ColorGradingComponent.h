#pragma once

#include "ComponentInterface.h"
#include "EditorTypes.h"
#include "ResourceManager.h"

class ColorGradingComponent : public ComponentInterface
{
public:
    static inline std::string ID = "colorGradingComponent";
    std::string getId() const override { return ID; }

    ColorGradingComponent(const Wolf::ResourceNonOwner<ResourceManager>& resourceManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline);

    void loadParams(Wolf::JSONReader& jsonReader) override;
    void activateParams() override;
    void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

    void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
    void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
    void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

    void saveCustom() const override {}

private:
    inline static const std::string TAB = "Color grading";
    Wolf::ResourceNonOwner<ResourceManager> m_resourceManager;
    Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;

    bool updateLUTImage();

    void onLUTImageMapChanged();
    ResourceManager::ResourceId m_lutImageResourceId = ResourceManager::NO_RESOURCE;
    bool m_lutImageUpdateRequested = true;
    EditorParamString m_lutImageParam = EditorParamString("LUT image", TAB, "Resources", [this]() { onLUTImageMapChanged(); }, EditorParamString::ParamStringType::FILE_IMG);

    std::array<EditorParamInterface*, 1> m_editorParams =
    {
        &m_lutImageParam
    };
};
