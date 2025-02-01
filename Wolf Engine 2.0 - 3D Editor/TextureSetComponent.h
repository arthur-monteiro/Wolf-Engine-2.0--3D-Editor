#pragma once

#include "ComponentInterface.h"
#include "EditorTypesTemplated.h"
#include "TextureSetEditor.h"
#include "ParameterGroupInterface.h"

class TextureSetComponent : public ComponentInterface
{
public:
	static inline std::string ID = "textureSetComponent";
	std::string getId() const override { return ID; }

	TextureSetComponent(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, 
		const std::function<void(ComponentInterface*)>& requestReloadCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	bool requiresInputs() const override { return false; }

	uint32_t getTextureSetIdx() const;

private:
	inline static const std::string TAB = "Texture Set";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialGPUManager;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;

	bool m_paramsLoaded = false;

	// Data
	class TextureSet : public ParameterGroupInterface, public Notifier
	{
	public:
		TextureSet();
		TextureSet(const TextureSet&) = delete;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override;
		uint32_t getTextureSetIdx() const { return m_textureSetEditor.getTextureSetIdx(); }

	private:
		TextureSetEditor m_textureSetEditor;
	};
	EditorParamGroup<TextureSet> m_textureSet = EditorParamGroup<TextureSet>("Textures", TAB, "Textures");

	// Sampling common
	void onSamplingModeChanged();
	bool m_changeSamplingModeRequested = false;
	EditorParamEnum m_samplingMode = EditorParamEnum({ "Mesh texture coordinates", "Triplanar" }, "Sampling mode", TAB, "Sampling", [this]() { onSamplingModeChanged(); });

	// Mesh texture coordinates
	void onTextureCoordsScaleChanged();
	bool m_changeTextureCoordsScaleRequested = false;
	EditorParamVector2 m_textureCoordsScale = EditorParamVector2("Scale", TAB, "Texture coordinates", 0.0f, 8.0f, [this]() { onTextureCoordsScaleChanged(); });

	// Triplanar
	void onTriplanarScaleChanged();
	bool m_changeTriplanarScaleRequested = false;
	EditorParamVector3 m_triplanarScale = EditorParamVector3("Scale", TAB, "Triplanar", 0.0f, 8.0f, [this]() { onTriplanarScaleChanged(); });

	std::array<EditorParamInterface*, 2> m_alwaysVisibleParams =
	{
		&m_textureSet,
		&m_samplingMode
	};

	std::array<EditorParamInterface*, 1> m_textureCoordsParams =
	{
		&m_textureCoordsScale
	};

	std::array<EditorParamInterface*, 1> m_triplanarParams =
	{
		&m_triplanarScale
	};

	std::array<EditorParamInterface*, 4> m_allParams =
	{
		&m_textureSet,
		&m_samplingMode,
		&m_textureCoordsScale,
		&m_triplanarScale
	};
};

