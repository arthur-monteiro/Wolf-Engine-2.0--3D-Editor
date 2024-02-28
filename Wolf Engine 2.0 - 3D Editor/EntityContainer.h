#pragma once

#include <memory>
#include <vector>

#include "Entity.h"

class EntityContainer
{
public:
	~EntityContainer();

	void addEntity(Entity* entity);
	void moveToNextFrame();

	void clear();

	std::vector<Wolf::ResourceUniqueOwner<Entity>>& getEntities() { return m_currentEntities; }

private:
	std::vector<Wolf::ResourceUniqueOwner<Entity>> m_currentEntities;
	std::vector<Wolf::ResourceUniqueOwner<Entity>> m_newEntities;
};