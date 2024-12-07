#include "TextureSetComponent.h"

#include "EditorParamsHelper.h"
#include "TextureSetLoader.h"

TextureSetComponent::TextureSetComponent(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, 
                                         const std::function<void(ComponentInterface*)>& requestReloadCallback)
	: m_materialGPUManager(materialsGPUManager), m_editorConfiguration(editorConfiguration), m_requestReloadCallback(requestReloadCallback)
{
	m_textureSet.get().subscribe(this, [this]() { });

	m_triplanarScale = glm::vec3(1.0f);
}

void TextureSetComponent::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<TextureSet>(jsonReader, ID, m_allParams);
	m_textureSet.get().subscribe(this, [this]() { m_requestReloadCallback(this); });
}

void TextureSetComponent::activateParams()
{
	for (EditorParamInterface* editorParam : m_allParams)
	{
		editorParam->activate();
	}
}

void TextureSetComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_alwaysVisibleParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}

	if (m_samplingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::TextureSetInfo::SamplingMode::TRIPLANAR))
	{
		for (const EditorParamInterface* editorParam : m_triplanarParams)
		{
			editorParam->addToJSON(outJSON, tabCount, false);
		}
	}
}

void TextureSetComponent::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
	if (m_changeSamplingModeRequested)
	{
		if (uint32_t textureSetIdx = m_textureSet.get().getTextureSetIdx(); textureSetIdx != 0)
		{
			m_materialGPUManager->changeSamplingModeBeforeFrame(textureSetIdx, static_cast<Wolf::MaterialsGPUManager::TextureSetInfo::SamplingMode>(static_cast<uint32_t>(m_samplingMode)));
			m_changeSamplingModeRequested = false;
		}
	}

	if (m_changeTriplanarScaleRequested)
	{
		if (uint32_t textureSetIdx = m_textureSet.get().getTextureSetIdx(); textureSetIdx != 0)
		{
			m_materialGPUManager->changeTriplanarScaleBeforeFrame(textureSetIdx, m_triplanarScale);
			m_changeTriplanarScaleRequested = false;
		}
	}

	m_textureSet.get().updateBeforeFrame(m_materialGPUManager, m_editorConfiguration);
}

uint32_t TextureSetComponent::getTextureSetIdx() const
{
	return m_textureSet.get().getTextureSetIdx();
}

// TODO: this is duplicated in particle at least, should be good to merge (maybe with contamination materials too)
TextureSetComponent::TextureSet::TextureSet() : ParameterGroupInterface(TextureSetComponent::TAB), m_textureSetEditor(TextureSetComponent::TAB, "Material", Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::GGX)
{
	m_name = "Material";

	//m_materialCacheInfo.materialInfo = m_materialsInfo.data();
	m_textureSetEditor.subscribe(this, [this]() { notifySubscribers(); });
}

void TextureSetComponent::TextureSet::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	m_textureSetEditor.updateBeforeFrame(materialGPUManager, editorConfiguration);
}

void TextureSetComponent::TextureSet::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	m_textureSetEditor.getAllParams(out);
}

void TextureSetComponent::TextureSet::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	m_textureSetEditor.getAllVisibleParams(out);
}

bool TextureSetComponent::TextureSet::hasDefaultName() const
{
	return true;
}

void TextureSetComponent::onSamplingModeChanged()
{
	m_requestReloadCallback(this);
	m_changeSamplingModeRequested = true;
}

void TextureSetComponent::onTriplanarScaleChanged()
{
	m_changeTriplanarScaleRequested = true;
}
