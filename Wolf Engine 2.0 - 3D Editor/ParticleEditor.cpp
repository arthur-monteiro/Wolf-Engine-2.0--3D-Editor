#include "ParticleEditor.h"

#include "AssetId.h"
#include "AssetManager.h"
#include "EditorParamsHelper.h"
#include "Entity.h"

ParticleEditor::ParticleEditor(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, AssetManager* assetManager)
: m_materialGPUManager(materialsGPUManager), m_assetManager(assetManager)
{
	m_flipBookSizeX = 1;
	m_flipBookSizeY = 1;
}

void ParticleEditor::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_editorParams);
}

void ParticleEditor::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void ParticleEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void ParticleEditor::updateBeforeFrame()
{
	if (m_waitingForMaterialToLoad)
	{
		if (m_assetManager->isMaterialLoaded(m_materialAssetId))
		{
			m_materialIdx = m_assetManager->getMaterialEditor(m_materialAssetId)->getMaterialGPUIdx();
			m_waitingForMaterialToLoad = false;

			notifySubscribers();
		}
	}
}

uint32_t ParticleEditor::getMaterialIdx() const
{
	return m_materialIdx;
}

void ParticleEditor::onMaterialAssetChanged()
{
	if (static_cast<std::string>(m_materialAssetParam) == "")
		return;

	AssetId assetId = m_assetManager->getAssetIdForPath(m_materialAssetParam);
	if (!m_assetManager->isMaterial(assetId))
	{
		Wolf::Debug::sendWarning("Asset is not a material");
		m_materialAssetParam = "";
	}

	m_materialAssetId = assetId;
	m_waitingForMaterialToLoad = true;
	notifySubscribers();
}
