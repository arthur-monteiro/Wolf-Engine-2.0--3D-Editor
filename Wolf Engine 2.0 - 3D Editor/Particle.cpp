#include "Particle.h"

#include "EditorParamsHelper.h"

Particle::Particle(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration) :
	m_materialGPUManager(materialsGPUManager), m_editorConfiguration(editorConfiguration)
{
	m_particleMaterial.get().subscribe(this, [this]() { notifySubscribers(); });

	m_flipBookSizeX = 1;
	m_flipBookSizeY = 1;
}

void Particle::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams<ParticleMaterial>(jsonReader, ID, m_editorParams);
}

void Particle::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void Particle::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void Particle::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
	m_particleMaterial.get().updateBeforeFrame(m_materialGPUManager, m_editorConfiguration);
}

Particle::ParticleMaterial::ParticleMaterial() : ParameterGroupInterface(Particle::TAB), m_materialsInfo(1), m_materialEditor(Particle::TAB, "", m_materialCacheInfo)
{
	m_name = DEFAULT_NAME;

	m_materialCacheInfo.materialInfo = m_materialsInfo.data();
	m_materialEditor.subscribe(this, [this]() { notifySubscribers(); });
}

void Particle::ParticleMaterial::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager,	const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	m_materialEditor.updateBeforeFrame(materialGPUManager, editorConfiguration);

	if (m_materialIdx == 0)
	{
		m_materialIdx = materialGPUManager->getCurrentMaterialCount();
		materialGPUManager->addNewMaterials(m_materialsInfo);
		m_materialCacheInfo = materialGPUManager->getMaterialsCacheInfo().back();

		m_materialEditor.setMaterialId(m_materialIdx);

		notifySubscribers();
	}
}

void Particle::ParticleMaterial::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	m_materialEditor.getAllParams(out);
}

void Particle::ParticleMaterial::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	m_materialEditor.getAllVisibleParams(out);
}

bool Particle::ParticleMaterial::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}
