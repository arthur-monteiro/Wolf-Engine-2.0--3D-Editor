#include "EntityContainer.h"

EntityContainer::EntityContainer()
{
	m_currentEntities.reserve(MAX_ENTITY_COUNT);
}

EntityContainer::~EntityContainer()
{
	clear();
}

void EntityContainer::addEntity(Entity* entity)
{
	uint32_t idx = m_currentEntities.size() + m_newEntities.size();
	entity->setIdx(idx);
	m_newEntities.emplace_back(entity);
}

void EntityContainer::moveToNextFrame(const std::function<ComponentInterface* (const std::string&)>& instanciateComponent)
{
	const uint32_t sizeBeforePush = static_cast<uint32_t>(m_currentEntities.size());
	for (Wolf::ResourceUniqueOwner<Entity>& entity : m_newEntities)
	{
		m_currentEntities.emplace_back(entity.release());
	}
	if (m_currentEntities.size() > MAX_ENTITY_COUNT)
	{
		Wolf::Debug::sendCriticalError("There are more entities than supported. All unique owner pointers has been changed resulting in garbage references for non owners");
	}
	m_newEntities.clear();

	for (uint32_t i = sizeBeforePush; i < m_currentEntities.size(); ++i)
	{
		m_currentEntities[i]->loadParams(instanciateComponent);
	}
}

void EntityContainer::clear()
{
	for (const Wolf::ResourceUniqueOwner<Entity>& entity : m_currentEntities)
	{
		entity->removeAllComponents();
	}

	for (const Wolf::ResourceUniqueOwner<Entity>& entity : m_newEntities)
	{
		entity->removeAllComponents();
	}

	m_currentEntities.clear();
	m_newEntities.clear();
}

void EntityContainer::findEntitiesWithCenterInSphere(const Wolf::BoundingSphere& sphere, std::vector<Wolf::ResourceNonOwner<Entity>>& out)
{
	for (Wolf::ResourceUniqueOwner<Entity>& entity : m_currentEntities)
	{
		if (glm::distance(entity->getBoundingSphere().getCenter(), sphere.getCenter()) < sphere.getRadius())
		{
			out.push_back(entity.createNonOwnerResource());
		}
	}
}

void EntityContainer::removeEntity(Entity* entity)
{
	for (uint32_t i = 0; i < m_currentEntities.size(); ++i)
	{
		if (m_currentEntities[i].isSame(entity))
		{
			m_currentEntities.erase(m_currentEntities.begin() + i);
			return;
		}
	}
}
