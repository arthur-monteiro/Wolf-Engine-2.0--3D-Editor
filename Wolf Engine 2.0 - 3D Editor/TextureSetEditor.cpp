#include "TextureSetEditor.h"

#include "EditorParamsHelper.h"
#include "ImageFileLoader.h"
#include "TextureSetLoader.h"
#include "MipMapGenerator.h"

TextureSetEditor::TextureSetEditor(const std::string& tab, const std::string& category, Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode shadingMode)
	: m_albedoPathParam("Albedo file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_normalPathParam("Normal file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_roughnessParam("Roughness file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_metalnessParam("Metalness file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_aoParam("AO file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_anisoStrengthParam("Aniso Strength file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_sixWaysLightmap0("6 ways light map 0", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_sixWaysLightmap1("6 ways light map 1", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_enableAlpha("Enable alpha",tab, category, [this]() { onTextureChanged(); }),
	m_alphaPathParam("Alpha map", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_shadingMode(Wolf::MaterialsGPUManager::MaterialInfo::SHADING_MODE_STRING_LIST, "Shading Mode", tab, category, [this]() { onShadingModeChanged(); }, false, true)
{
	m_shadingMode = static_cast<uint32_t>(shadingMode);
}

void TextureSetEditor::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	if (m_textureSetCacheInfo.textureSetIdx == 0)
	{
		materialGPUManager->lockTextureSets();

		m_textureSetCacheInfo.textureSetIdx = materialGPUManager->getCurrentTextureSetCount();
		materialGPUManager->addNewTextureSet(m_textureSetInfo);

		materialGPUManager->unlockTextureSets();
	}
	else if (m_textureChanged) // wait a frame between the creation of the texture set and the texture update
	{
		if (m_shadingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::GGX) || m_shadingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::AnisoGGX))
		{
			Wolf::TextureSetLoader::TextureSetFileInfoGGX materialFileInfo{};
			materialFileInfo.name = "Custom material";
			materialFileInfo.albedo = static_cast<std::string>(m_albedoPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_albedoPathParam);
			materialFileInfo.normal = static_cast<std::string>(m_normalPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_normalPathParam);
			materialFileInfo.roughness = static_cast<std::string>(m_roughnessParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_roughnessParam);
			materialFileInfo.metalness = static_cast<std::string>(m_metalnessParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_metalnessParam);
			materialFileInfo.ao = static_cast<std::string>(m_aoParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_aoParam);

			if (m_textureSetCacheInfo.imageNames.empty() || m_textureSetCacheInfo.imageNames[0] != materialFileInfo.albedo.substr(materialFileInfo.albedo.size() - m_textureSetCacheInfo.imageNames[0].size()))
			{
				Wolf::TextureSetLoader::OutputLayout outputLayout{};
				outputLayout.albedoCompression = m_enableAlpha ? Wolf::ImageCompression::Compression::BC3 : Wolf::ImageCompression::Compression::BC1;
				outputLayout.normalCompression = Wolf::ImageCompression::Compression::BC5;

				Wolf::TextureSetLoader materialLoader(materialFileInfo, outputLayout, false);

				materialLoader.transferImageTo(0, m_textureSetInfo.images[0]);
				m_textureSetInfo.slicesFolders[0] = materialLoader.getOutputSlicesFolder(0);

				materialLoader.transferImageTo(1, m_textureSetInfo.images[1]);
				m_textureSetInfo.slicesFolders[1] = materialLoader.getOutputSlicesFolder(1);

				materialLoader.transferImageTo(2, m_textureSetInfo.images[2]);
				m_textureSetInfo.slicesFolders[2] = materialLoader.getOutputSlicesFolder(2);

				materialGPUManager->lockTextureSets();
				materialGPUManager->changeExistingTextureSetBeforeFrame(m_textureSetCacheInfo, m_textureSetInfo);
				materialGPUManager->unlockTextureSets();

				notifySubscribers();
			}
		}
		else if (m_shadingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::SixWaysLighting))
		{
			Wolf::TextureSetLoader::TextureSetFileInfoSixWayLighting materialFileInfo;
			materialFileInfo.name = "Custom material";
			materialFileInfo.tex0 = static_cast<std::string>(m_sixWaysLightmap0).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_sixWaysLightmap0);
			materialFileInfo.tex1 = static_cast<std::string>(m_sixWaysLightmap1).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_sixWaysLightmap1);

			Wolf::TextureSetLoader materialLoader(materialFileInfo, false);
			materialLoader.transferImageTo(0, m_textureSetInfo.images[0]);
			materialLoader.transferImageTo(1, m_textureSetInfo.images[1]);

			materialGPUManager->changeExistingTextureSetBeforeFrame(m_textureSetCacheInfo, m_textureSetInfo);

			notifySubscribers();
		}
		else if (m_shadingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::AlphaOnly))
		{
			Wolf::TextureSetLoader::TextureSetFileInfoAlphaOnly materialFileInfo;
			materialFileInfo.name = "Custom material";
			materialFileInfo.alphaMap = static_cast<std::string>(m_alphaPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_alphaPathParam);

			Wolf::TextureSetLoader materialLoader(materialFileInfo, false);
			materialLoader.transferImageTo(0, m_textureSetInfo.images[0]);

			materialGPUManager->changeExistingTextureSetBeforeFrame(m_textureSetCacheInfo, m_textureSetInfo);

			notifySubscribers();
		}

		m_textureChanged = false;
	}
}

void TextureSetEditor::activateParams()
{
	Wolf::Debug::sendError("Not implemented");
}

void TextureSetEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast)
{
	Wolf::Debug::sendError("Not implemented");
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

	// Shape specific params
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
}

uint32_t TextureSetEditor::getTextureSetIdx() const
{
	return m_textureSetCacheInfo.textureSetIdx;
}

void TextureSetEditor::onTextureChanged()
{
	m_textureChanged = true;
}

void TextureSetEditor::onShadingModeChanged()
{
	notifySubscribers(0);
	m_shadingModeChanged = true;
}