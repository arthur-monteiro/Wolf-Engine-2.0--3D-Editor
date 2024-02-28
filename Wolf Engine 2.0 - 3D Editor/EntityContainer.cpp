#include "EntityContainer.h"

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
