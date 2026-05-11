#pragma once

#include <array>

#include "AssetId.h"
#include "ComponentInterface.h"
#include "EditorTypes.h"

class AssetManager;

class TextureSetEditor : public ComponentInterface
{
public:
	static inline std::string ID = "textureSetEditor";
	std::string getId() const override { return ID; }

	TextureSetEditor(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, AssetManager* assetManager, AssetId textureSetAssetId);
	TextureSetEditor(const TextureSetEditor&) = delete;

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	// TODO: inherit from something else than ComponentInterface to avoid having that update override
	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	void updateBeforeFrame();

	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void saveCustom() const override {}

	void copy(const Wolf::ResourceNonOwner<TextureSetEditor>& other);

	void getAllParams(std::vector<EditorParamInterface*>& out) const;
	void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const;

	uint32_t getTextureSetIdx() const;

	std::string getAlbedoPath() const { return m_albedoPathParam; }
	std::string getNormalPath() const { return m_normalPathParam; }
	std::string getRoughnessPath() const { return m_roughnessParam; }
	std::string getMetalnessPath() const { return m_metalnessParam; }
	std::string getAOPath() const { return m_aoParam; }
	std::string getAnisoStrengthPath() const { return m_anisoStrengthParam; }
	uint32_t getShadingMode() const { return m_shadingMode; }

	void setAlbedoPath(const std::string& albedoPath) { m_albedoPathParam = albedoPath; }
	void setNormalPath(const std::string& normalPath) { m_normalPathParam = normalPath; }
	void setRoughnessPath(const std::string& roughnessPath) { m_roughnessParam = roughnessPath; }
	void setMetalnessPath(const std::string& metalnessPath) { m_metalnessParam = metalnessPath; }
	void setAOPath(const std::string& aoPath) { m_aoParam = aoPath; }
	void setAnisoStrengthPath(const std::string& anisoStrengthPath) { m_anisoStrengthParam = anisoStrengthPath; }
	void setShadingMode(uint32_t shadingMode) { m_shadingMode = shadingMode; }

private:
	inline static const std::string TAB = "Texture Set";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialGPUManager;
	AssetManager* m_assetManager;
	AssetId m_textureSetAssetId;

	Wolf::MaterialsGPUManager::TextureSetInfo computeTextureSetInfo();

	void onShadingModeChanged();

	void updateAssetId(AssetId& outAssetId, EditorParamString& param);

	void onAlbedoAssetChanged();
	AssetId m_albedoAssetId = NO_ASSET;
	EditorParamString m_albedoPathParam = EditorParamString("Albedo file", TAB, "Textures", [this]() { onAlbedoAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onNormalAssetChanged();
	AssetId m_normalAssetId = NO_ASSET;
	EditorParamString m_normalPathParam = EditorParamString("Normal file", TAB, "Textures", [this]() { onNormalAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onRoughnessAssetChanged();
	AssetId m_roughnessAssetId = NO_ASSET;
	EditorParamString m_roughnessParam = EditorParamString("Roughness file", TAB, "Textures", [this]() { onRoughnessAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onMetalnessAssetChanged();
	AssetId m_metalnessAssetId = NO_ASSET;
	EditorParamString m_metalnessParam = EditorParamString("Metalness file", TAB, "Textures", [this]() { onMetalnessAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onAOAssetChanged();
	AssetId m_aoAssetId = NO_ASSET;
	EditorParamString m_aoParam = EditorParamString("AO file", TAB, "Textures", [this]() { onAOAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onAnisoAssetChanged();
	AssetId m_anisoAssetId = NO_ASSET;
	EditorParamString m_anisoStrengthParam = EditorParamString("Aniso Strength file", TAB, "Textures", [this]() { onAnisoAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onSixWayLightmap0AssetChanged();
	AssetId m_sixWayLightmap0AssetId = NO_ASSET;
	EditorParamString m_sixWaysLightmap0 = EditorParamString("6 ways light map 0", TAB, "Textures", [this]() { onSixWayLightmap0AssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onSixWayLightmap1AssetChanged();
	AssetId m_sixWayLightmap1AssetId = NO_ASSET;
	EditorParamString m_sixWaysLightmap1 = EditorParamString("6 ways light map 1", TAB, "Textures", [this]() { onSixWayLightmap1AssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onAlphaAssetChanged();
	AssetId m_alphaMapAssetId = NO_ASSET;
	EditorParamBool m_enableAlpha = EditorParamBool("Enable alpha",TAB, "Alpha", [this]() { /* TODO */ });
	EditorParamString m_alphaPathParam = EditorParamString("Alpha map", TAB, "Alpha", [this]() { onAlphaAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	EditorParamEnum m_shadingMode = EditorParamEnum(Wolf::MaterialsGPUManager::MaterialInfo::SHADING_MODE_STRING_LIST, "Shading Mode", TAB, "Shading", [this]() { onShadingModeChanged(); }, false, true);

	EditorParamUInt m_textureSetIdx = EditorParamUInt("Texture set index", TAB, "Debug", 0, 1000, EditorParamUInt::ParamUIntType::NUMBER, false, true);

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

	std::array<EditorParamInterface*, 6> m_shadingModeGGXParams
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessParam,
		&m_metalnessParam,
		&m_aoParam,
		&m_enableAlpha,
	};

	std::array<EditorParamInterface*, 7> m_shadingModeGGXAnisoParams
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessParam,
		&m_metalnessParam,
		&m_aoParam,
		&m_anisoStrengthParam,
		&m_enableAlpha,
	};

	std::array<EditorParamInterface*, 2> m_shadingModeSixWaysLightingParams
	{
		&m_sixWaysLightmap0,
		&m_sixWaysLightmap1
	};

	std::array<EditorParamInterface*, 1> m_shadingModeAlphaOnlyParams
	{
		&m_alphaPathParam
	};

	std::array<EditorParamInterface*, 3> m_alwaysVisibleParams
	{
		&m_shadingMode,
		&m_samplingMode,
		&m_textureSetIdx
	};

	std::array<EditorParamInterface*, 1> m_textureCoordsParams =
	{
		&m_textureCoordsScale
	};

	std::array<EditorParamInterface*, 1> m_triplanarParams =
	{
		&m_triplanarScale
	};

	std::array<EditorParamInterface*, 15> m_allParams =
	{
		&m_albedoPathParam,
		&m_normalPathParam,
		&m_roughnessParam,
		&m_metalnessParam,
		&m_aoParam,
		&m_anisoStrengthParam,
		&m_sixWaysLightmap0,
		&m_sixWaysLightmap1,
		&m_enableAlpha,
		&m_alphaPathParam,
		&m_shadingMode,
		&m_samplingMode,
		&m_textureCoordsScale,
		&m_triplanarScale,
		&m_textureSetIdx
	};

	bool m_shadingModeChanged = false;
	bool m_textureChanged = false;
};
