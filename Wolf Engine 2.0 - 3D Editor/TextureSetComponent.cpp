#include "TextureSetComponent.h"

#include "EditorParamsHelper.h"
#include "TextureSetLoader.h"

TextureSetComponent::TextureSetComponent(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, 
                                         const std::function<void(ComponentInterface*)>& requestReloadCallback, const Wolf::ResourceNonOwner<AssetManager>& assetManager)
	: m_materialGPUManager(materialsGPUManager), m_editorConfiguration(editorConfiguration), m_requestReloadCallback(requestReloadCallback), m_assetManager(assetManager)
{
	m_textureSet.get().subscribe(this, [this](Flags) { });

	m_textureCoordsScale = glm::vec2(1.0f);
	m_triplanarScale = glm::vec3(1.0f);
}

void TextureSetComponent::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<TextureSet>(jsonReader, ID, m_allParams);
	m_textureSet.get().subscribe(this, [this](Flags) { m_requestReloadCallback(this); });

	m_paramsLoaded = true;
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

	if (m_samplingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::TextureSetInfo::SamplingMode::TEXTURE_COORDS))
	{
		for (const EditorParamInterface* editorParam : m_textureCoordsParams)
		{
			editorParam->addToJSON(outJSON, tabCount, false);
		}
	}

	if (m_samplingMode == static_cast<uint32_t>(Wolf::MaterialsGPUManager::TextureSetInfo::SamplingMode::TRIPLANAR))
	{
		for (const EditorParamInterface* editorParam : m_triplanarParams)
		{
			editorParam->addToJSON(outJSON, tabCount, false);
		}
	}
}

void TextureSetComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	// ReSharper disable once CppDFAConstantConditions
	if (m_paramsLoaded)
	{
		// ReSharper disable once CppDFAUnreachableCode
		if (m_changeSamplingModeRequested)
		{
			if (uint32_t textureSetIdx = m_textureSet.get().getTextureSetIdx(); textureSetIdx != 0)
			{
				m_materialGPUManager->changeSamplingModeBeforeFrame(textureSetIdx, static_cast<Wolf::MaterialsGPUManager::TextureSetInfo::SamplingMode>(static_cast<uint32_t>(m_samplingMode)));
				m_changeSamplingModeRequested = false;
			}
		}

		if (uint32_t textureSetIdx = m_textureSet.get().getTextureSetIdx(); textureSetIdx != 0)
		{
			bool scaleChangedRequested = false;
			glm::vec3 scale;
			switch (static_cast<uint32_t>(m_samplingMode))
			{
			case 0: // mesh coordinates
			{
				if (m_changeTextureCoordsScaleRequested)
				{
					scale = glm::vec3(static_cast<glm::vec2>(m_textureCoordsScale), 1.0f);
					scaleChangedRequested = true;
					m_changeTextureCoordsScaleRequested = false;
				}
				break;
			}
			case 1: // triplanar
			{
				if (m_changeTriplanarScaleRequested)
				{
					scale = static_cast<glm::vec3>(m_triplanarScale);
					scaleChangedRequested = true;
					m_changeTriplanarScaleRequested = false;
				}
				break;
			}
			default:
				Wolf::Debug::sendError("Unhandled sampling mode");
			}

			if (scaleChangedRequested)
			{
				m_materialGPUManager->changeScaleBeforeFrame(textureSetIdx, scale);
				m_changeTriplanarScaleRequested = m_changeTextureCoordsScaleRequested = false;
			}
		}

		m_textureSet.get().updateBeforeFrame(m_materialGPUManager, m_editorConfiguration, m_assetManager);
	}
}

uint32_t TextureSetComponent::getTextureSetIdx() const
{
	return m_textureSet.get().getTextureSetIdx();
}

TextureSetComponent::TextureSet::TextureSet() : ParameterGroupInterface(TextureSetComponent::TAB), m_textureSetEditor(TextureSetComponent::TAB, "Material", Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::GGX)
{
	m_name = "Material";

	//m_materialCacheInfo.materialInfo = m_materialsInfo.data();
	m_textureSetEditor.subscribe(this, [this](Flags) { notifySubscribers(); });
}

void TextureSetComponent::TextureSet::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const Wolf::ResourceReference<AssetManager>& assetManager)
{
	m_textureSetEditor.updateBeforeFrame(materialGPUManager, editorConfiguration, assetManager);
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

void TextureSetComponent::onTextureCoordsScaleChanged()
{
	m_changeTextureCoordsScaleRequested = true;
}

void TextureSetComponent::onTriplanarScaleChanged()
{
	m_changeTriplanarScaleRequested = true;
}
