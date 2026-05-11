#include "TextureSetEditor.h"

#include "AssetManager.h"
#include "EditorParamsHelper.h"
#include "ImageFileLoader.h"
#include "MaterialsGPUManager.h"
#include "TextureSetLoader.h"
#include "MipMapGenerator.h"

TextureSetEditor::TextureSetEditor(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, AssetManager* assetManager, AssetId textureSetAssetId)
: m_materialGPUManager(materialGPUManager), m_assetManager(assetManager), m_textureSetAssetId(textureSetAssetId)
{
	m_shadingMode = static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::GGX);
	m_textureCoordsScale = glm::vec2(1.0f);
}

void TextureSetEditor::updateBeforeFrame()
{
	if (m_textureSetIdx == 0)
	{
		m_materialGPUManager->lockTextureSets();

		m_textureSetIdx = m_materialGPUManager->getCurrentTextureSetCount();
		m_materialGPUManager->addNewTextureSet(computeTextureSetInfo());

		m_materialGPUManager->unlockTextureSets();
	}
	else if (m_textureChanged) // wait a frame between the creation of the texture set and the texture update
	{
		m_materialGPUManager->lockTextureSets();
		// TODO
		//m_materialGPUManager->changeExistingTextureSetBeforeFrame(m_textureSetCacheInfo, m_textureSetInfo);
		m_materialGPUManager->unlockTextureSets();

		m_textureChanged = false;
	}
}

void TextureSetEditor::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_allParams);
	m_textureSetIdx = 0;
}

void TextureSetEditor::activateParams()
{
	for (EditorParamInterface* editorParam : m_allParams)
 	{
		editorParam->activate();
 	}
}

void TextureSetEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	std::vector<EditorParamInterface*> visibleParams;
	getAllVisibleParams(visibleParams);

	for (const EditorParamInterface* editorParam : visibleParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void TextureSetEditor::copy(const Wolf::ResourceNonOwner<TextureSetEditor>& other)
{
	m_albedoPathParam = static_cast<std::string>(other->m_albedoPathParam);
	m_normalPathParam = static_cast<std::string>(other->m_normalPathParam);
	m_roughnessParam = static_cast<std::string>(other->m_roughnessParam);
	m_metalnessParam = static_cast<std::string>(other->m_metalnessParam);
	m_aoParam = static_cast<std::string>(other->m_aoParam);

	m_textureSetIdx = static_cast<uint32_t>(other->m_textureSetIdx);
}

void TextureSetEditor::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void TextureSetEditor::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	for (EditorParamInterface* editorParam : m_alwaysVisibleParams)
	{
		out.push_back(editorParam);
	}

	// Shading mode specific params
	switch (static_cast<Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode>(static_cast<uint32_t>(m_shadingMode)))
	{
		case Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::GGX:
		{
			for (EditorParamInterface* editorParam : m_shadingModeGGXParams)
			{
				out.push_back(editorParam);
			}
			break;
		}
		case Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::AnisoGGX:
		{
			for (EditorParamInterface* editorParam : m_shadingModeGGXAnisoParams)
			{
				out.push_back(editorParam);
			}
			break;
		}
		case Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::SixWaysLighting:
		{
			for (EditorParamInterface* editorParam : m_shadingModeSixWaysLightingParams)
			{
				out.push_back(editorParam);
			}
			break;
		}
		case Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::AlphaOnly:
		{
			for (EditorParamInterface* editorParam : m_shadingModeAlphaOnlyParams)
			{
				out.push_back(editorParam);
			}
			break;
		}
		default:
			Wolf::Debug::sendError("Undefined shading mode");
			break;
	}

	if (m_samplingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::TextureSetInfo::SamplingMode::TEXTURE_COORDS))
	{
		for (EditorParamInterface* editorParam : m_textureCoordsParams)
		{
			out.push_back(editorParam);
		}
	}

	if (m_samplingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::TextureSetInfo::SamplingMode::TRIPLANAR))
	{
		for (EditorParamInterface* editorParam : m_triplanarParams)
		{
			out.push_back(editorParam);
		}
	}
}

uint32_t TextureSetEditor::getTextureSetIdx() const
{
	return m_textureSetIdx;
}

Wolf::MaterialsGPUManager::TextureSetInfo TextureSetEditor::computeTextureSetInfo()
{
	Wolf::MaterialsGPUManager::TextureSetInfo r{};

	if (m_shadingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::GGX) || m_shadingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::AnisoGGX))
	{
		TextureSetLoader::TextureSetAssetsInfoGGX textureSetFileInfo{};
		textureSetFileInfo.m_name = "Custom material";
		textureSetFileInfo.m_albedoAssetId = m_albedoAssetId;
		textureSetFileInfo.m_normalAssetId = m_normalAssetId;
		textureSetFileInfo.m_roughnessAssetId = m_roughnessAssetId;
		textureSetFileInfo.m_metalnessAssetId = m_metalnessAssetId;
		textureSetFileInfo.m_aoAssetId = m_aoAssetId;

		TextureSetLoader::OutputLayout outputLayout{};
		if (m_enableAlpha)
		{
			Wolf::Debug::sendCriticalError("Doesn't seem to be supported?");
		}
		outputLayout.albedoCompression = m_enableAlpha ? Wolf::ImageCompression::Compression::BC3 : Wolf::ImageCompression::Compression::BC1;
		outputLayout.normalCompression = Wolf::ImageCompression::Compression::BC5;

		TextureSetLoader textureSetLoader(textureSetFileInfo, outputLayout, true, m_assetManager, m_textureSetAssetId);

		if (AssetId assetId = textureSetLoader.getImageAssetId(0); assetId != NO_ASSET)
		{
			r.images[0] = m_assetManager->getImage(assetId, Wolf::Format::BC1_RGB_SRGB_BLOCK);
			r.slicesFolders[0] = m_assetManager->getImageSlicesFolder(assetId);
		}

		if (AssetId assetId = textureSetLoader.getImageAssetId(1); assetId != NO_ASSET)
		{
			r.images[1] = m_assetManager->getImage(assetId, Wolf::Format::BC5_UNORM_BLOCK);
			r.slicesFolders[1] = m_assetManager->getImageSlicesFolder(assetId);
		}

		if (AssetId assetId = textureSetLoader.getImageAssetId(2); assetId != NO_ASSET)
		{
			r.images[2] = m_assetManager->getCombinedImage(assetId, Wolf::Format::BC3_UNORM_BLOCK);
			r.slicesFolders[2] = m_assetManager->getCombinedImageSlicesFolder(assetId);
		}

		r.scale = glm::vec3(static_cast<glm::vec2>(m_textureCoordsScale), 1.0f);
	}
	else if (m_shadingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::SixWaysLighting))
	{
		TextureSetLoader::TextureSetAssetsInfoSixWayLighting materialFileInfo;
		materialFileInfo.name = "Custom material";
		materialFileInfo.m_tex0AssetId = m_sixWayLightmap0AssetId;
		materialFileInfo.m_tex1AssetId = m_sixWayLightmap1AssetId;

		TextureSetLoader textureSetLoader(materialFileInfo, m_assetManager);
		r.images[0] = m_assetManager->getImage(textureSetLoader.getImageAssetId(0), Wolf::Format::R8G8B8A8_UNORM);
		r.images[1] = m_assetManager->getImage(textureSetLoader.getImageAssetId(1), Wolf::Format::R8G8B8A8_UNORM);
	}
	else if (m_shadingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::AlphaOnly))
	{
		TextureSetLoader::TextureSetAssetInfoAlphaOnly materialFileInfo;
		materialFileInfo.name = "Custom material";
		materialFileInfo.m_alphaMapAssetId = m_alphaMapAssetId;

		TextureSetLoader textureSetLoader(materialFileInfo, m_assetManager);
		r.images[0] = m_assetManager->getImage(textureSetLoader.getImageAssetId(0), Wolf::Format::R8G8B8A8_UNORM);
	}

	return r;
}

void TextureSetEditor::onShadingModeChanged()
{
	notifySubscribers(0);
	m_shadingModeChanged = true;
}

void TextureSetEditor::updateAssetId(AssetId& outAssetId , EditorParamString& param)
{
	if (static_cast<std::string>(param) == "")
		return;

	AssetId assetId = m_assetManager->getAssetIdForPath(param);
	if (!m_assetManager->isImage(assetId))
	{
		Wolf::Debug::sendWarning("Asset is not an image");
		param = "";
	}

	outAssetId = assetId;
	m_textureChanged = true;
	notifySubscribers();
}

void TextureSetEditor::onAlbedoAssetChanged()
{
	updateAssetId(m_albedoAssetId, m_albedoPathParam);
}

void TextureSetEditor::onNormalAssetChanged()
{
	updateAssetId(m_normalAssetId, m_normalPathParam);
}

void TextureSetEditor::onRoughnessAssetChanged()
{
	updateAssetId(m_roughnessAssetId, m_roughnessParam);
}

void TextureSetEditor::onMetalnessAssetChanged()
{
	updateAssetId(m_metalnessAssetId, m_metalnessParam);
}

void TextureSetEditor::onAOAssetChanged()
{
	updateAssetId(m_aoAssetId, m_aoParam);
}

void TextureSetEditor::onAnisoAssetChanged()
{
	updateAssetId(m_anisoAssetId, m_anisoStrengthParam);
}

void TextureSetEditor::onSixWayLightmap0AssetChanged()
{
	updateAssetId(m_sixWayLightmap0AssetId, m_sixWaysLightmap0);
}

void TextureSetEditor::onSixWayLightmap1AssetChanged()
{
	updateAssetId(m_sixWayLightmap1AssetId, m_sixWaysLightmap1);
}

void TextureSetEditor::onAlphaAssetChanged()
{
	updateAssetId(m_alphaMapAssetId, m_alphaPathParam);
}

void TextureSetEditor::onSamplingModeChanged()
{
}

void TextureSetEditor::onTextureCoordsScaleChanged()
{
}

void TextureSetEditor::onTriplanarScaleChanged()
{
}
