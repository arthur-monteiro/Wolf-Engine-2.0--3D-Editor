#pragma once

#include <map>

#include <PhysicsManager.h>
#include <ResourceNonOwner.h>

#include "AssetManager.h"

class Entity;

class EditorPhysicsManager
{
public:
	EditorPhysicsManager(const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager);

	struct PhysicsMeshInfo
	{
		Wolf::ResourceNonOwner<Wolf::Physics::Shape> shape;
		glm::mat4 transform;
	};
	void addMeshes(const std::vector<PhysicsMeshInfo>& physicsMeshes, Entity* entity);
	void removeMeshesForEntity(Entity* entity);

	void addDebugInfo(DebugRenderingManager& debugRenderingManager);

private:
	Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager> m_physicsManager;
	std::mutex m_meshMutex;

	struct InfoByEntity
	{
		Wolf::Physics::PhysicsManager::DynamicShapeId shapeId;

		// Debug info
		Wolf::Physics::PhysicsShapeType shapeType;

		// Rectangle debug info
		glm::mat4 rectangleTransform;

		// Box debug info
		glm::vec3 boxAABBMin;
		glm::vec3 boxAABBMax;
		glm::vec3 boxRotation;
	};
	std::map<Entity*, std::vector<InfoByEntity>> m_infoByEntityArray;
};

