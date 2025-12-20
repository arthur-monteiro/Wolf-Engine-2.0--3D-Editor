#include "Particle.h"

#include "EditorParamsHelper.h"
#include "Entity.h"
#include "MaterialComponent.h"

Particle::Particle(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration,
                   const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback) :
	m_materialGPUManager(materialsGPUManager), m_editorConfiguration(editorConfiguration), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
	m_flipBookSizeX = 1;
	m_flipBookSizeY = 1;
}

void Particle::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_editorParams);
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

void Particle::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	if (m_materialEntity && !m_materialNotificationRegistered)
	{
		if (const Wolf::NullableResourceNonOwner<MaterialComponent> materialComponent = (*m_materialEntity)->getComponent<MaterialComponent>())
		{
			materialComponent->subscribe(this, [this](Flags) { notifySubscribers(); });
			m_materialNotificationRegistered = true;

			notifySubscribers();
		}
	}
}

uint32_t Particle::getMaterialIdx() const
{
	if (m_materialEntity)
	{
		if (const Wolf::ResourceNonOwner<MaterialComponent> materialComponent = (*m_materialEntity)->getComponent<MaterialComponent>())
		{
			return materialComponent->getMaterialIdx();
		}
	}

	return 0;
}

void Particle::onMaterialEntityChanged()
{
	if (static_cast<std::string>(m_materialEntityParam) != "")
	{
		m_materialEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_materialEntityParam)));
		m_materialNotificationRegistered = false;
	}
}
