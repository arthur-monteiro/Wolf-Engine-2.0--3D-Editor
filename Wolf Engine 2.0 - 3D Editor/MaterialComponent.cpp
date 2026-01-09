#include "MaterialComponent.h"

#include "EditorParamsHelper.h"
#include "Entity.h"
#include "TextureSetComponent.h"

class TextureSetComponent;

MaterialComponent::MaterialComponent(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
	const std::function<void(ComponentInterface*)>& requestReloadCallback,
	const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
	: m_materialsGPUManager(materialsGPUManager), m_requestReloadCallback(requestReloadCallback), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
}

void MaterialComponent::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<TextureSet>(jsonReader, ID, m_allParams);
}

void MaterialComponent::activateParams()
{
	for (EditorParamInterface* editorParam : m_allParams)
	{
		editorParam->activate();
	}
}

void MaterialComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_allParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void MaterialComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
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

		if (m_materialIdx == DEFAULT_MATERIAL_IDX)
		{
			Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
			materialInfo.textureSetInfos[indexOfTextureSetInMaterial].textureSetIdx = textureSetGPUIdx;
			materialInfo.textureSetInfos[indexOfTextureSetInMaterial].strength = strength;

			m_materialsGPUManager->lockMaterials();
			m_materialIdx = m_materialsGPUManager->getCurrentMaterialCount();
			m_materialsGPUManager->addNewMaterial(materialInfo);
			m_materialsGPUManager->unlockMaterials();

			notifySubscribers();

			// TODO: delay the rest
			return;
		}
		else
		{
			m_materialsGPUManager->changeTextureSetIdxBeforeFrame(m_materialIdx, indexOfTextureSetInMaterial, textureSetGPUIdx);
			m_materialsGPUManager->changeStrengthBeforeFrame(m_materialIdx, indexOfTextureSetInMaterial, strength);
		}
	}

	m_textureSetChangedIndices.clear();
	m_textureSetChangedIndices.swap(delayedIndices);

	if (m_shadingModeChanged && m_materialIdx != DEFAULT_MATERIAL_IDX)
	{
		m_materialsGPUManager->changeMaterialShadingModeBeforeFrame(m_materialIdx, m_shadingMode);
		m_shadingModeChanged = false;
	}
}

Wolf::NullableResourceNonOwner<TextureSetComponent> MaterialComponent::getTextureSetComponent(uint32_t textureSetIdx) const
{
	return m_textureSets[textureSetIdx].getTextureSetComponent();
}

void MaterialComponent::onShadingModeChanged()
{
	m_shadingModeChanged = true;
}

void MaterialComponent::onTextureSetChanged(uint32_t textureSetIdx)
{
	m_textureSetChangedIndices.push_back(textureSetIdx);
}

MaterialComponent::TextureSet::TextureSet() : ParameterGroupInterface(TAB)
{
	m_name = DEFAULT_NAME;
}

void MaterialComponent::TextureSet::setGetEntityFromLoadingPathCallback(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
{
	m_getEntityFromLoadingPathCallback = getEntityFromLoadingPathCallback;
}

void MaterialComponent::TextureSet::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void MaterialComponent::TextureSet::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

bool MaterialComponent::TextureSet::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

uint32_t MaterialComponent::TextureSet::getTextureSetIdx() const
{
	if (m_textureSetComponent)
	{
		return m_textureSetComponent->getTextureSetIdx();
	}

	return NO_TEXTURE_SET_IDX;
}

void MaterialComponent::TextureSet::onTextureSetEntityChanged()
{
	if (static_cast<std::string>(m_textureSetEntityParam).empty())
	{
		m_textureSetComponent.release();
	}
	else
	{
		Wolf::ResourceNonOwner<Entity> entity = m_getEntityFromLoadingPathCallback(m_textureSetEntityParam);
		m_textureSetComponent = entity->getComponent<TextureSetComponent>();
		if (m_textureSetComponent)
		{
			if (entity->isSubscribed(this))
				entity->unsubscribe(this);

			m_textureSetComponent->subscribe(this, [this](Flags) { notifySubscribers(); });
		}
		else if (!entity->isSubscribed(this))
		{
			entity->subscribe(this, [this](Flags) { onTextureSetEntityChanged(); });
		}
	}

	notifySubscribers();
}

void MaterialComponent::onTextureSetAdded()
{
	m_textureSets.back().setGetEntityFromLoadingPathCallback(m_getEntityFromLoadingPathCallback);

	uint32_t idx = static_cast<uint32_t>(m_textureSets.size()) - 1;
	m_textureSets.back().subscribe(this, [this, idx](Flags) { onTextureSetChanged(idx); });

	m_requestReloadCallback(this);
}
