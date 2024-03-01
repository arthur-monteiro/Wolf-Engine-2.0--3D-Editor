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
	m_newEntities.emplace_back(entity);
}

void EntityContainer::moveToNextFrame()
{
	for (Wolf::ResourceUniqueOwner<Entity>& entity : m_newEntities)
	{
		m_currentEntities.emplace_back(entity.release());
	}
	if (m_currentEntities.size() > MAX_ENTITY_COUNT)
	{
		Wolf::Debug::sendCriticalError("There are more entities than supported. All unique owner pointers has been changed resulting in garbage references for non owners");
	}
	m_newEntities.clear();
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
