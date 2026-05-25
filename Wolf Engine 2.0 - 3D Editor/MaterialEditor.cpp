#include "MaterialEditor.h"

#include "AssetManager.h"
#include "EditorParamsHelper.h"

MaterialEditor::MaterialEditor(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, AssetManager* assetManager)
	: m_materialsGPUManager(materialsGPUManager), m_assetManager(assetManager)
{
	m_overrideColor = false;
	m_color = glm::vec3(1.0f);
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
			materialInfo.m_textureSetsInfo[indexOfTextureSetInMaterial].m_textureSetIdx = textureSetGPUIdx;
			materialInfo.m_textureSetsInfo[indexOfTextureSetInMaterial].m_strength = strength;
			materialInfo.m_shadingMode = static_cast<Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode>(static_cast<uint32_t>(m_shadingMode));
			materialInfo.m_color = static_cast<glm::vec3>(m_color);

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

	if (m_colorChanged && m_materialGPUIdx != DEFAULT_MATERIAL_IDX)
	{
		m_materialsGPUManager->changeMaterialColorBeforeFrame(m_materialGPUIdx, m_color);
		m_colorChanged = false;
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

glm::vec3 MaterialEditor::TextureSet::computeColor() const
{
	if (m_textureSetAssetId != NO_ASSET)
	{
		AssetId albedoAssetId = m_assetManager->getTextureSetEditor(m_textureSetAssetId)->getAlbedoAssetId();
		if (albedoAssetId != NO_ASSET)
		{
			AssetImageInterface::LoadingRequest loadingRequest{};
			loadingRequest.m_canBeVirtualized = false;
			loadingRequest.m_format = Wolf::Format::R8G8B8A8_UNORM;
			loadingRequest.m_keepDataMode = AssetImageInterface::KeepDataMode::ONLY_CPU;
			loadingRequest.m_loadMips = true;
			m_assetManager->requestImageLoading(albedoAssetId, loadingRequest, true);

			Wolf::ResourceNonOwner<AssetImage> assetImage = m_assetManager->getAssetImage(albedoAssetId);
			Wolf::Extent3D extent = assetImage->getExtent(loadingRequest.m_format);
			const uint8_t* data = assetImage->getMipData(0, loadingRequest.m_format);
			const Wolf::ImageCompression::RGBA8* pixels = reinterpret_cast<const Wolf::ImageCompression::RGBA8*>(data);

			glm::vec3 averageColor(0.0f);
			uint32_t totalPixels = extent.width * extent.height * extent.depth;
			for (uint32_t i = 0; i < totalPixels; i++)
			{
				glm::vec3 currentPixel(pixels[i].r, pixels[i].g, pixels[i].b);
				averageColor += (currentPixel - averageColor) / static_cast<float>(i + 1);
			}
			averageColor /= 255.0f;

			assetImage->deleteImageData(loadingRequest.m_format);

			return averageColor;
		}
	}

	return glm::vec3(1.0f);
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

void MaterialEditor::setColor(glm::vec3 color)
{
	m_color = color;
}

void MaterialEditor::recomputeColor()
{
	uint32_t textureSetCount = static_cast<uint32_t>(m_textureSets.size());
	glm::vec3 averageColor(0.0f);
	for (uint32_t textureSetIdx = 0; textureSetIdx < textureSetCount; ++textureSetIdx)
	{
		glm::vec3 textureSetColor = m_textureSets[textureSetIdx].computeColor();
		averageColor += (textureSetColor - averageColor) / static_cast<float>(textureSetIdx + 1);
	}
	m_color = averageColor;
}

void MaterialEditor::onOverrideColorChanged()
{
	m_color.setReadOnly(!m_overrideColor);
}

void MaterialEditor::onColorChanged()
{
	m_colorChanged = true;
}
