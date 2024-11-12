#pragma once

#include <memory>
#include <vector>

#include "Entity.h"

class EntityContainer
{
public:
	EntityContainer();
	~EntityContainer();

	void addEntity(Entity* entity);
	void moveToNextFrame(const std::function<ComponentInterface* (const std::string&)>& instanciateComponent);

	void clear();

	std::vector<Wolf::ResourceUniqueOwner<Entity>>& getEntities() { return m_currentEntities; }

private:
	static constexpr uint32_t MAX_ENTITY_COUNT = 4096;

	std::vector<Wolf::ResourceUniqueOwner<Entity>> m_currentEntities;
	std::vector<Wolf::ResourceUniqueOwner<Entity>> m_newEntities;
};