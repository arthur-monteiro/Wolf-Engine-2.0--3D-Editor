#pragma once

#include "Entity.h"

class SystemManager;

class GraphicSettingsFakeEntity : public Entity
{
public:
    GraphicSettingsFakeEntity(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, SystemManager* systemManager, const std::function<void(ComponentInterface*)>& requestReloadCallback);

    void activateParams() override;
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
    SystemManager* m_systemManager;
    std::function<void(ComponentInterface*)> m_requestReloadCallback;

    void forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString);

    bool m_skyCubeMapResolutionChanged = false;
    EditorParamUInt m_skyCubeMapResolution = EditorParamUInt("Cube map resolution", TAB, "Sky", 4, 4096, [this]() { m_skyCubeMapResolutionChanged = true; });

    [[nodiscard]] GameContext::ShadowTechnique computeShadowTechnique() const;
    void updateShadowTechnique();
    EditorParamEnum m_shadowTechnique = EditorParamEnum({ "Cascaded Shadow Mapping", "Ray Traced" }, "Shadow technique", TAB, "Shadows",
    [this]() { updateShadowTechnique();});

    // CSM settings
    void onCSMFarChanged();
    EditorParamFloat m_csmFar = EditorParamFloat("Cascade shadow mapping max range", TAB, "Shadows", 25.0f, 5000.0f, [this]() { onCSMFarChanged(); });

    void onVoxelGIDebugChanged();
    EditorParamBool m_enableVoxelGIDebug = EditorParamBool("Enable voxel GI debug", TAB, "Voxel GI", [this]() { onVoxelGIDebugChanged(); });

    std::array<EditorParamInterface*, 3> m_alwaysVisibleParams =
    {
        &m_skyCubeMapResolution,
        &m_shadowTechnique,
        &m_enableVoxelGIDebug
    };

    std::array<EditorParamInterface*, 1> m_csmParams =
    {
        &m_csmFar
    };
};
