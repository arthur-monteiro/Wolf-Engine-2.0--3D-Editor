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

	[[nodiscard]] std::vector<Wolf::ResourceUniqueOwner<Entity>>& getEntities() { return m_currentEntities; }
	void findEntitiesWithCenterInSphere(const Wolf::BoundingSphere& sphere, std::vector<Wolf::ResourceNonOwner<Entity>>& out);
	void removeEntity(Entity* entity);

private:
	static constexpr uint32_t MAX_ENTITY_COUNT = 4096;

	std::vector<Wolf::ResourceUniqueOwner<Entity>> m_currentEntities;
	std::vector<Wolf::ResourceUniqueOwner<Entity>> m_newEntities;
};