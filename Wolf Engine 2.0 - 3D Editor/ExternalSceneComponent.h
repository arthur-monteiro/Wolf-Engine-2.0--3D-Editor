#pragma once

#include "AssetManager.h"
#include "EditorModelInterface.h"
#include "EditorTypes.h"

class ExternalSceneComponent : public EditorModelInterface
{
public:
    static inline std::string ID = "externalSceneComponent";
    std::string getId() const override { return ID; }

    ExternalSceneComponent(const Wolf::ResourceNonOwner<AssetManager>& assetManager);

    void loadParams(Wolf::JSONReader& jsonReader) override;
    void activateParams() override;
    void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

    void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
    void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
    void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

    void saveCustom() const override {}

    bool getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) override;
    bool getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList) override;
    bool getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos) override;

    Wolf::AABB getAABB() const override;
    Wolf::BoundingSphere getBoundingSphere() const override;

private:
    bool areAllMeshLoaded();

    inline static const std::string TAB = "Scene";

    Wolf::ResourceNonOwner<AssetManager> m_assetManager;

    bool m_isWaitingForSceneLoading = false;
    void requestSceneLoading();
    EditorParamString m_loadingPathParam = EditorParamString("Mesh", TAB, "Loading", [this] { requestSceneLoading(); }, EditorParamString::ParamStringType::FILE_EXTERNAL_SCENE);

    std::array<EditorParamInterface*, 1> m_editorParams =
    {
        &m_loadingPathParam
    };

    AssetId m_sceneAssetId = NO_ASSET;

    // TEMP
    std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, ExternalSceneComponent>> m_defaultPipelineSet;
};
