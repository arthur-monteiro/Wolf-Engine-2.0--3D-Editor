#include "MaterialEditor.h"

#include "AssetManager.h"
#include "EditorParamsHelper.h"

MaterialEditor::MaterialEditor(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, AssetManager* assetManager)
	: m_materialsGPUManager(materialsGPUManager), m_assetManager(assetManager)
{
}

void MaterialEditor::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<TextureSet>(jsonReader.getRoot()->getPropertyObject(ID), ID, m_allParams);
}

void MaterialEditor::activateParams()
{
	for (EditorParamInterface* editorParam : m_allParams)
	{
		editorParam->activate();
	}
}

void MaterialEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_allParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void MaterialEditor::updateBeforeFrame()
{
	std::vector<uint32_t> delayedIndices;
	for (uint32_t indexOfTextureSetInMaterial : m_textureSetChangedIndices)
	{
		const TextureSet& textureSet = m_textureSets[indexOfTextureSetInMaterial];

		uint32_t textureSetGPUIdx = textureSet.getTextureSetIdx();
		if (textureSetGPUIdx == TextureSet::NO_TEXTURE_SET_IDX)
		{
			textureSetGPUIdx = 0;
		}
		else if (textureSetGPUIdx == 0) // texture set not loaded yet
		{
			delayedIndices.push_back(indexOfTextureSetInMaterial);
			continue;
		}
		float strength = textureSet.getStrength();

		if (m_materialGPUIdx == DEFAULT_MATERIAL_IDX)
		{
			Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
			materialInfo.textureSetInfos[indexOfTextureSetInMaterial].textureSetIdx = textureSetGPUIdx;
			materialInfo.textureSetInfos[indexOfTextureSetInMaterial].strength = strength;
			materialInfo.shadingMode = static_cast<Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode>(static_cast<uint32_t>(m_shadingMode));

			m_materialsGPUManager->lockMaterials();
			m_materialGPUIdx = m_materialsGPUManager->getCurrentMaterialCount();
			m_materialsGPUManager->addNewMaterial(materialInfo);
			m_materialsGPUManager->unlockMaterials();

			notifySubscribers();

			// TODO: delay the rest
			return;
		}
		else
		{
			m_materialsGPUManager->changeTextureSetIdxBeforeFrame(m_materialGPUIdx, indexOfTextureSetInMaterial, textureSetGPUIdx);
			m_materialsGPUManager->changeStrengthBeforeFrame(m_materialGPUIdx, indexOfTextureSetInMaterial, strength);
		}
	}

	m_textureSetChangedIndices.clear();
	m_textureSetChangedIndices.swap(delayedIndices);

	if (m_shadingModeChanged && m_materialGPUIdx != DEFAULT_MATERIAL_IDX)
	{
		m_materialsGPUManager->changeMaterialShadingModeBeforeFrame(m_materialGPUIdx, m_shadingMode);
		m_shadingModeChanged = false;
	}
}

void MaterialEditor::addTextureSet(const std::string& textureSetPath, float strength)
{
	TextureSet& addedTextureSet = m_textureSets.emplace_back();
	addedTextureSet.setTextureSetPath(textureSetPath);
	addedTextureSet.setStrength(strength);
}

void MaterialEditor::onShadingModeChanged()
{
	m_shadingModeChanged = true;
}

void MaterialEditor::onTextureSetChanged(uint32_t textureSetIdx)
{
	m_textureSetChangedIndices.push_back(textureSetIdx);
}

MaterialEditor::TextureSet::TextureSet() : ParameterGroupInterface(TAB, "Texture set")
{
	m_name = DEFAULT_NAME;
}

void MaterialEditor::TextureSet::setAssetManager(AssetManager* assetManager)
{
	m_assetManager = assetManager;
}

void MaterialEditor::TextureSet::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void MaterialEditor::TextureSet::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

bool MaterialEditor::TextureSet::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

void MaterialEditor::TextureSet::setTextureSetPath(const std::string& path)
{
	m_textureSetAssetParam = path;
}

uint32_t MaterialEditor::TextureSet::getTextureSetIdx() const
{
	if (m_textureSetAssetId != NO_ASSET)
	{
		return m_assetManager->getTextureSetEditor(m_textureSetAssetId)->getTextureSetIdx();
	}

	return NO_TEXTURE_SET_IDX;
}

void MaterialEditor::TextureSet::onTextureSetAssetChanged()
{
	if (static_cast<std::string>(m_textureSetAssetParam) == "")
		return;

	AssetId assetId = m_assetManager->getAssetIdForPath(m_textureSetAssetParam);
	if (!m_assetManager->isTextureSet(assetId))
	{
		Wolf::Debug::sendWarning("Asset is not a texture set");
		m_textureSetAssetParam = "";
	}

	m_textureSetAssetId = assetId;
	notifySubscribers();
}

void MaterialEditor::onTextureSetAdded()
{
	m_textureSets.back().setAssetManager(m_assetManager);

	uint32_t idx = static_cast<uint32_t>(m_textureSets.size()) - 1;
	m_textureSets.back().subscribe(this, [this, idx](Flags) { onTextureSetChanged(idx); });
}
