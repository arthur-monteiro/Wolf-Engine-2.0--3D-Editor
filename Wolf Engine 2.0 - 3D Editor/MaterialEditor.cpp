#include "MaterialEditor.h"

#include "EditorParamsHelper.h"
#include "ImageFileLoader.h"
#include "MaterialLoader.h"
#include "MipMapGenerator.h"

MaterialEditor::MaterialEditor(const std::string& tab, const std::string& category)
	: m_albedoPathParam("Albedo file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
      m_normalPathParam("Normal file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
      m_roughnessParam("Roughness file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
      m_metalnessParam("Metalness file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG),
      m_aoParam("AO file", tab, category, [this]() { onTextureChanged(); }, EditorParamString::ParamStringType::FILE_IMG)
{
}

void MaterialEditor::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	if (m_updateNeeded)
	{
		Wolf::MaterialLoader::MaterialFileInfo materialFileInfo{};
		materialFileInfo.name = "Custom material";
		materialFileInfo.albedo = static_cast<std::string>(m_albedoPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_albedoPathParam);
		materialFileInfo.normal = static_cast<std::string>(m_normalPathParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_normalPathParam);
		materialFileInfo.roughness = static_cast<std::string>(m_roughnessParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_roughnessParam);
		materialFileInfo.metalness = static_cast<std::string>(m_metalnessParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_metalnessParam);
		materialFileInfo.ao = static_cast<std::string>(m_aoParam).empty() ? "" : editorConfiguration->computeFullPathFromLocalPath(m_aoParam);

		Wolf::MaterialLoader materialLoader(materialFileInfo, Wolf::MaterialLoader::InputMaterialLayout::EACH_TEXTURE_A_FILE, false);
		m_albedoImage.reset(materialLoader.releaseImage(0));

		std::vector<Wolf::DescriptorSetGenerator::ImageDescription> images(Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL);
		for (Wolf::DescriptorSetGenerator::ImageDescription& image : images)
		{
			image.imageView = nullptr;
		}

		if (m_albedoImage)
		{
			images[0].imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
			images[0].imageView = m_albedoImage->getDefaultImageView();
		}

		m_materialId = materialGPUManager->getCurrentMaterialCount();
		materialGPUManager->addNewMaterials(images);

		m_updateNeeded = false;
		notifySubscribers();
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

void MaterialEditor::onTextureChanged()
{
	m_updateNeeded = true;
}
