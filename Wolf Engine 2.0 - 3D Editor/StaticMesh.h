#pragma once

#include <PipelineSet.h>

#include "EditorModelInterface.h"
#include "AssetManager.h"

class StaticMesh : public EditorModelInterface
{
public:
	static inline std::string ID = "staticMesh";
	std::string getId() const override { return ID; }

	StaticMesh(const Wolf::ResourceNonOwner<AssetManager>& assetManager);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	bool getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) override;
	bool getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos) override;
	bool getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	void setLoadingPath(const std::string& loadingPath) { m_meshAssetParam = loadingPath; }
	void setInfoFromParent(AssetId modelAssetId);

	Wolf::AABB getAABB() const override;
	Wolf::BoundingSphere getBoundingSphere() const override;

	void saveCustom() const override {}

private:
	inline static const std::string TAB = "Mesh";
	Wolf::ResourceNonOwner<AssetManager> m_assetManager;
	AssetId m_modelAssetId = NO_ASSET;

	bool m_isWaitingForMeshLoading = false;

	void onMeshAssetChanged();
	EditorParamString m_meshAssetParam = EditorParamString("Mesh", TAB, "Loading", [this] { onMeshAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	EditorParamEnum m_drawLODType = EditorParamEnum({ "Default", "Sloppy" }, "LOD type for draw", TAB, "Quality", [this]() { notifySubscribers(); });

	void onRayTracedWorldLODTypeChanged();
	EditorParamEnum m_rayTracedWorldLODType = EditorParamEnum({ "Default", "Sloppy" }, "LOD type for ray traced world", TAB, "Quality", [this]() { onRayTracedWorldLODTypeChanged(); });
	EditorParamUInt m_rayTracedWorldLOD = EditorParamUInt("LOD for ray traced world", TAB, "Quality", 0, 0, [this]() { notifySubscribers(); });

	std::array<EditorParamInterface*, 4> m_alwaysVisibleEditorParams =
	{
		&m_meshAssetParam,
		&m_drawLODType,
		&m_rayTracedWorldLODType,
		&m_rayTracedWorldLOD
	};

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, StaticMesh>> m_defaultPipelineSet;
};
