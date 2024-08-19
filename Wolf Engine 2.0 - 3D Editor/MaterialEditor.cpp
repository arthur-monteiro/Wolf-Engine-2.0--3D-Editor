#include "MaterialEditor.h"

#include "EditorParamsHelper.h"
#include "ImageFileLoader.h"
#include "MaterialLoader.h"
#include "MipMapGenerator.h"

MaterialEditor::MaterialEditor(const std::string& tab, const std::string& category, Wolf::MaterialsGPUManager::MaterialCacheInfo& materialCacheInfo)
	: m_albedoPathParam("Albedo file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_normalPathParam("Normal file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_roughnessParam("Roughness file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_metalnessParam("Metalness file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_aoParam("AO file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_anisoStrengthParam("Aniso Strength file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_sixWaysLightmap0("6 ways light map 0", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_sixWaysLightmap1("6 ways light map 1", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_enableAlpha("Enable alpha",tab, category, [this]() { onTextureChanged(); }),
	m_shadingMode({ "GGX", "Anisotropic GGX", "6 Ways Lighting" }, "Shading Mode", tab, category, [this]() { onShadingModeChanged(); }),
	m_materialCacheInfo(materialCacheInfo)
{
}

void MaterialEditor::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	if (m_shadingModeChanged)
	{
		materialGPUManager->changeMaterialShadingModeBeforeFrame(m_materialId, m_shadingMode);
		m_materialCacheInfo.materialInfo->shadingMode = m_shadingMode;
		m_shadingModeChanged = false;

		notifySubscribers();
	}

	if (m_textureChanged)
	{
		if (m_shadingMode == ShadingMode::GGX || m_shadingMode == ShadingMode::AnisoGGX)
		{
			Wolf::MaterialLoader::MaterialFileInfoGGX materialFileInfo{};
			materialFileInfo.name = "Custom material";
			materialFileInfo.albedo = static_cast<std::string>(m_albedoPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_albedoPathParam);
			materialFileInfo.normal = static_cast<std::string>(m_normalPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_normalPathParam);
			materialFileInfo.roughness = static_cast<std::string>(m_roughnessParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_roughnessParam);
			materialFileInfo.metalness = static_cast<std::string>(m_metalnessParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_metalnessParam);
			materialFileInfo.ao = static_cast<std::string>(m_aoParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_aoParam);

			if (m_materialCacheInfo.materialInfo->imageNames.empty() || m_materialCacheInfo.materialInfo->imageNames[0] != materialFileInfo.albedo.substr(materialFileInfo.albedo.size() - m_materialCacheInfo.materialInfo->imageNames[0].size()))
			{
				Wolf::MaterialLoader::OutputLayout outputLayout;
				outputLayout.albedoCompression = m_enableAlpha ? Wolf::ImageCompression::Compression::BC3 : Wolf::ImageCompression::Compression::BC1;

				Wolf::MaterialLoader materialLoader(materialFileInfo, outputLayout, false);
				m_materialCacheInfo.materialInfo->images[0].reset(materialLoader.releaseImage(0));

				if (m_materialId != 0)
				{
					materialGPUManager->changeExistingMaterialBeforeFrame(m_materialId, m_materialCacheInfo);
				}

				notifySubscribers();
			}
		}
		else if (m_shadingMode == ShadingMode::SixWaysLighting)
		{
			Wolf::MaterialLoader::MaterialFileInfoSixWayLighting materialFileInfo;
			materialFileInfo.name = "Custom material";
			materialFileInfo.tex0 = static_cast<std::string>(m_sixWaysLightmap0).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_sixWaysLightmap0);
			materialFileInfo.tex1 = static_cast<std::string>(m_sixWaysLightmap1).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_sixWaysLightmap1);

			Wolf::MaterialLoader materialLoader(materialFileInfo, false);
			m_materialCacheInfo.materialInfo->images[0].reset(materialLoader.releaseImage(0));
			m_materialCacheInfo.materialInfo->images[1].reset(materialLoader.releaseImage(1));

			if (m_materialId != 0)
			{
				materialGPUManager->changeExistingMaterialBeforeFrame(m_materialId, m_materialCacheInfo);
			}

			notifySubscribers();
		}

		m_textureChanged = false;
	}
}

void MaterialEditor::activateParams()
{
	Wolf::Debug::sendError("Not implemented");
}

void MaterialEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast)
{
	Wolf::Debug::sendError("Not implemented");
}


void MaterialEditor::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void MaterialEditor::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	for (EditorParamInterface* editorParam : m_alwaysVisibleParams)
	{
		out.push_back(editorParam);
	}

	// Shape specific params
	switch (static_cast<uint32_t>(m_shadingMode))
	{
	case ShadingMode::GGX:
	{
		for (EditorParamInterface* editorParam : m_shadingModeGGXParams)
		{
			out.push_back(editorParam);
		}
		break;
	}
	case ShadingMode::AnisoGGX:
	{
		for (EditorParamInterface* editorParam : m_shadingModeGGXAnisoParams)
		{
			out.push_back(editorParam);
		}
		break;
	}
	case ShadingMode::SixWaysLighting:
	{
		for (EditorParamInterface* editorParam : m_shadingModeSixWaysLighting)
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

void MaterialEditor::onTextureChanged()
{
	m_textureChanged = true;
}

void MaterialEditor::onShadingModeChanged()
{
	notifySubscribers();
	m_shadingModeChanged = true;
}