#pragma once

#include "AssetManager.h"
#include "EditorModelInterface.h"
#include "EditorTypes.h"

class ExternalSceneComponent : public EditorModelInterface
{
public:
    static inline std::string ID = "externalSceneComponent";
    std::string getId() const override { return ID; }

    ExternalSceneComponent(const Wolf::ResourceNonOwner<AssetManager>& assetManager, const std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)>& getEntityCallback,
        const std::function<Entity*(ComponentInterface*, const std::string&)>& createEntityCallback, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
        const std::function<void(ComponentInterface*)>& requestReloadCallback);

    void loadParams(Wolf::JSONReader& jsonReader) override;
    void activateParams() override;
    void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

    void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
    void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
    void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

    void saveCustom() const override {}

    bool getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) override { return true; }
    bool getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList) override { return true; }
    bool getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos) override { return true; }

    Wolf::AABB getAABB() const override;
    Wolf::BoundingSphere getBoundingSphere() const override;

private:
    bool areAllMeshLoaded();

    inline static const std::string TAB = "Scene";

    Wolf::ResourceNonOwner<AssetManager> m_assetManager;
    std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)> m_getEntityCallback;
    std::function<Entity*(ComponentInterface*, const std::string&)> m_createEntityCallback;
    Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
    std::function<void(ComponentInterface*)> m_requestReloadCallback;

    bool m_isWaitingForSceneLoading = false;
    void requestSceneLoading();
    EditorParamString m_loadingPathParam = EditorParamString("Mesh", TAB, "Loading", [this] { requestSceneLoading(); }, EditorParamString::ParamStringType::FILE_EXTERNAL_SCENE);

    std::array<EditorParamInterface*, 1> m_editorParams =
    {
        &m_loadingPathParam
    };

    AssetId m_sceneAssetId = NO_ASSET;
};
