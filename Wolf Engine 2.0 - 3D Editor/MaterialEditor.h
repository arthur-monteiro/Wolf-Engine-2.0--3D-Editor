#pragma once

#include "AssetId.h"
#include "ComponentInterface.h"
#include "EditorTypesTemplated.h"
#include "Notifier.h"
#include "ParameterGroupInterface.h"

class AssetManager;

class MaterialEditor : public ComponentInterface
{
public:
	static inline std::string ID = "materialEditor";
	std::string getId() const override { return ID; }

	MaterialEditor(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, AssetManager* assetManager);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	void updateBeforeFrame();
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void saveCustom() const override {}

	static constexpr uint32_t DEFAULT_MATERIAL_IDX = 0;
	uint32_t getMaterialGPUIdx() const { return m_materialGPUIdx; }

	void addTextureSet(const std::string& textureSetPath, float strength);
	uint32_t getTextureSetCount() const { return m_textureSets.size(); }

private:
	inline static const std::string TAB = "Material";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	AssetManager* m_assetManager;

	bool m_shadingModeChanged = false;
	void onShadingModeChanged();
	EditorParamEnum m_shadingMode = EditorParamEnum(Wolf::MaterialsGPUManager::MaterialInfo::SHADING_MODE_STRING_LIST, "Shading Mode", TAB, "Material", [this]() { onShadingModeChanged(); });

	void onTextureSetChanged(uint32_t textureSetIdx);
	std::vector<uint32_t> m_textureSetChangedIndices;
	class TextureSet : public ParameterGroupInterface, public Notifier
	{
	public:
		TextureSet();
		TextureSet(const TextureSet&) = delete;

		void setAssetManager(AssetManager* assetManager);

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override;

		void setTextureSetPath(const std::string& path);
		void setStrength(float strength) { m_strength = strength; }

		static constexpr uint32_t NO_TEXTURE_SET_IDX = -1;
		uint32_t getTextureSetIdx() const;
		float getStrength() const { return m_strength; }

	private:
		inline static const std::string DEFAULT_NAME = "New texture set";
		AssetManager* m_assetManager = nullptr;

		void onTextureSetAssetChanged();
		AssetId m_textureSetAssetId = NO_ASSET;
		EditorParamString m_textureSetAssetParam = EditorParamString("Texture set", TAB, "Texture set", [this]() { onTextureSetAssetChanged(); }, EditorParamString::ParamStringType::ASSET);
		EditorParamFloat m_strength = EditorParamFloat("Strength", TAB, "Material", 0.01f, 2.0f, [this]() { notifySubscribers(); });
		std::array<EditorParamInterface*, 2> m_allParams =
		{
			&m_textureSetAssetParam,
			&m_strength
		};
	};

	static constexpr uint32_t MAX_TEXTURE_SETS = 16;
	void onTextureSetAdded();
	EditorParamArray<TextureSet> m_textureSets = EditorParamArray<TextureSet>("Texture sets", TAB, "Material", MAX_TEXTURE_SETS, [this] { onTextureSetAdded(); });

	std::array<EditorParamInterface*, 2> m_allParams =
	{
		&m_shadingMode,
		&m_textureSets
	};

	uint32_t m_materialGPUIdx = DEFAULT_MATERIAL_IDX;
};

