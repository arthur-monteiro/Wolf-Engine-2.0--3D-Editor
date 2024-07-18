#include "MaterialEditor.h"

#include "EditorParamsHelper.h"
#include "ImageFileLoader.h"
#include "MaterialLoader.h"
#include "MipMapGenerator.h"

MaterialEditor::MaterialEditor(const std::string& tab, const std::string& category, Wolf::MaterialsGPUManager::MaterialCacheInfo& materialCacheInfo)
	: m_albedoPathParam("Albedo file", tab, category, [this]() { onAlbedoChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_normalPathParam("Normal file", tab, category, [this]() { onAlbedoChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_roughnessParam("Roughness file", tab, category, [this]() { onAlbedoChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_metalnessParam("Metalness file", tab, category, [this]() { onAlbedoChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_aoParam("AO file", tab, category, [this]() { onAlbedoChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_anisoStrengthParam("Aniso Strength file", tab, category, [this]() { onAlbedoChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
	m_shadingMode({ "GGX", "Anisotropic GGX" }, "Shading Mode", tab, category, [this]() { onShadingModeChanged(); }),
	m_materialCacheInfo(materialCacheInfo)
{
}

void MaterialEditor::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	if (m_shadingModeChanged)
	{
		materialGPUManager->changeMaterialShadingModeBeforeFrame(m_materialId, m_shadingMode);
	}

	if (m_textureChanged)
	{
		Wolf::MaterialLoader::MaterialFileInfo materialFileInfo{};
		materialFileInfo.name = "Custom material";
		materialFileInfo.albedo = static_cast<std::string>(m_albedoPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_albedoPathParam);
		materialFileInfo.normal = static_cast<std::string>(m_normalPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_normalPathParam);
		materialFileInfo.roughness = static_cast<std::string>(m_roughnessParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_roughnessParam);
		materialFileInfo.metalness = static_cast<std::string>(m_metalnessParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_metalnessParam);
		materialFileInfo.ao = static_cast<std::string>(m_aoParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_aoParam);

		if (m_materialCacheInfo.materialInfo->imageNames.empty() || m_materialCacheInfo.materialInfo->imageNames[0] != materialFileInfo.albedo.substr(materialFileInfo.albedo.size() - m_materialCacheInfo.materialInfo->imageNames[0].size()))
		{
			Wolf::MaterialLoader materialLoader(materialFileInfo, Wolf::MaterialLoader::InputMaterialLayout::EACH_TEXTURE_A_FILE, false);
			m_materialCacheInfo.materialInfo->images[0].reset(materialLoader.releaseImage(0));

			if (m_materialId != 0)
			{
				materialGPUManager->changeExistingMaterialBeforeFrame(m_materialId, m_materialCacheInfo);
			}

			notifySubscribers();
		}

		m_textureChanged = false;
	}
}

void MaterialEditor::activateParams() const
{
	for (EditorParamInterface* param : m_materialParams)
	{
		param->activate();
	}
}

void MaterialEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const
{
	::addParamsToJSON(outJSON, m_materialParams, isLast, tabCount);
}

void MaterialEditor::onAlbedoChanged()
{
	m_textureChanged = true;
}

void MaterialEditor::onShadingModeChanged()
{
	m_shadingModeChanged = true;
}
