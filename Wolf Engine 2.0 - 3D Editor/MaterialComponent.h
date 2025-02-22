#pragma once

#include "ComponentInterface.h"
#include "EditorTypesTemplated.h"
#include "Notifier.h"
#include "ParameterGroupInterface.h"

class MaterialComponent : public ComponentInterface, public Notifier
{
public:
	static inline std::string ID = "materialComponent";
	std::string getId() const override { return ID; }

	MaterialComponent(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const std::function<void(ComponentInterface*)>& requestReloadCallback,
		const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	bool requiresInputs() const override { return false; }
	void saveCustom() const override {}

	static constexpr uint32_t DEFAULT_MATERIAL_IDX = 0;
	uint32_t getMaterialIdx() const { return m_materialIdx; }


private:
	inline static const std::string TAB = "Material";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	bool m_shadingModeChanged = false;
	void onShadingModeChanged();
	EditorParamEnum m_shadingMode = EditorParamEnum({ "GGX", "Anisotropic GGX", "6 Ways Lighting" }, "Shading Mode", TAB, "Material", [this]() { onShadingModeChanged(); });

	void onTextureSetChanged(uint32_t textureSetIdx);
	std::vector<uint32_t> m_textureSetChangedIndices;
	class TextureSet : public ParameterGroupInterface, public Notifier
	{
	public:
		TextureSet();
		TextureSet(const TextureSet&) = delete;

		void setGetEntityFromLoadingPathCallback(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override;

		static constexpr uint32_t NO_TEXTURE_SET_IDX = -1;
		uint32_t getTextureSetIdx() const;
		float getStrength() const { return m_strength; }

	private:
		inline static const std::string DEFAULT_NAME = "New texture set";
		std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

		void onTextureSetEntityChanged();
		EditorParamString m_textureSetEntityParam = EditorParamString("Texture set entity", TAB, "Texture set", [this]() { onTextureSetEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);
		EditorParamFloat m_strength = EditorParamFloat("Strength", TAB, "Material", 0.01f, 2.0f, [this]() { notifySubscribers(); });
		std::array<EditorParamInterface*, 2> m_allParams =
		{
			&m_textureSetEntityParam,
			&m_strength
		};

		std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_textureSetEntity;
	};

	static constexpr uint32_t MAX_TEXTURE_SETS = 16;
	void onTextureSetAdded();
	EditorParamArray<TextureSet> m_textureSets = EditorParamArray<TextureSet>("Texture sets", TAB, "Material", MAX_TEXTURE_SETS, [this] { onTextureSetAdded(); });

	std::array<EditorParamInterface*, 2> m_allParams =
	{
		&m_shadingMode,
		&m_textureSets
	};

	uint32_t m_materialIdx = DEFAULT_MATERIAL_IDX;
};

