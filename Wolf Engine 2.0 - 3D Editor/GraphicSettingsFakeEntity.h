#pragma once

#include "Entity.h"

class GraphicSettingsFakeEntity : public Entity
{
public:
    GraphicSettingsFakeEntity(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline);

    void activateParams() const override;
    void fillJSONForParams(std::string& outJSON) override;

    void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager, const Wolf::ResourceNonOwner<EditorPhysicsManager>& editorPhysicsManager) override;

    void save() const override;

    const std::string& getName() const override { return m_name; }
    std::string computeEscapedLoadingPath() const override { return "materialsListId"; }
    bool isFake() const override { return true; }

private:
    std::string m_name = "Graphic Settings";
    inline static const std::string TAB = "Settings";
	Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;

    bool m_skyCubeMapResolutionChanged = false;
    EditorParamUInt m_skyCubeMapResolution = EditorParamUInt("Cube map resolution", TAB, "Sky", 4, 4096, [this]() { m_skyCubeMapResolutionChanged = true; });

    std::array<EditorParamInterface*, 1> m_editorParams =
    {
        &m_skyCubeMapResolution
    };
};
