#pragma once

#include "AssetId.h"
#include "ComponentInterface.h"
#include "EditorConfiguration.h"
#include "EditorTypesTemplated.h"
#include "EditorTypes.h"

class AssetManager;

class ParticleEditor : public ComponentInterface
{
public:
	static inline std::string ID = "particleEditor";
	std::string getId() const override { return ID; }

	ParticleEditor(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, AssetManager* assetManager);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	void updateBeforeFrame();
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void saveCustom() const override {}

	uint32_t getMaterialIdx() const;
	uint32_t getFlipBookSizeX() const { return m_flipBookSizeX; }
	uint32_t getFlipBookSizeY() const { return m_flipBookSizeY; }

private:
	inline static const std::string TAB = "Particle";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialGPUManager;
	AssetManager* m_assetManager;

	void onMaterialAssetChanged();
	AssetId m_materialAssetId = NO_ASSET;
	bool m_waitingForMaterialToLoad = false;
	uint32_t m_materialIdx = 0;
	EditorParamString m_materialAssetParam = EditorParamString("Material", TAB, "Material", [this]() { onMaterialAssetChanged(); }, EditorParamString::ParamStringType::ASSET);
	EditorParamUInt m_flipBookSizeX = EditorParamUInt("Flip book size X", TAB, "Material", 1, 8, [this]() { notifySubscribers(); });
	EditorParamUInt m_flipBookSizeY = EditorParamUInt("Flip book size Y", TAB, "Material", 1, 8, [this]() { notifySubscribers(); });

	std::array<EditorParamInterface*, 3> m_editorParams =
	{
		&m_materialAssetParam,
		&m_flipBookSizeX,
		&m_flipBookSizeY
	};
};
