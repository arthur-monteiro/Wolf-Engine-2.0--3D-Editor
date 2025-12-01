#include "EditorPhysicsManager.h"

#include "DebugRenderingManager.h"
#include "MathsUtilsEditor.h"
#include "glm/gtx/matrix_decompose.hpp"

EditorPhysicsManager::EditorPhysicsManager(const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager) : m_physicsManager(physicsManager)
{
}

void EditorPhysicsManager::addMeshes(const std::vector<PhysicsMeshInfo>& physicsMeshes, Entity* entity)
{
	removeMeshesForEntity(entity);

	m_meshMutex.lock();

	for (const PhysicsMeshInfo& physicsMeshInfo : physicsMeshes)
	{
		Wolf::Physics::PhysicsShapeType shapeType = physicsMeshInfo.shape->getType();

		if (shapeType == Wolf::Physics::PhysicsShapeType::Rectangle)
		{
			const Wolf::Physics::Rectangle& shapeAsRectangle = dynamic_cast<const Wolf::Physics::Rectangle&>(*physicsMeshInfo.shape);

			glm::vec3 p0 = physicsMeshInfo.transform * glm::vec4(shapeAsRectangle.getP0(), 1.0f);
			glm::vec3 p1 = physicsMeshInfo.transform * glm::vec4(shapeAsRectangle.getP0() + shapeAsRectangle.getS1(), 1.0f);
			glm::vec3 p2 = physicsMeshInfo.transform * glm::vec4(shapeAsRectangle.getP0() + shapeAsRectangle.getS2(), 1.0f);

			Wolf::Physics::PhysicsManager::DynamicShapeId shapeId = m_physicsManager->addDynamicRectangle(Wolf::Physics::Rectangle(shapeAsRectangle.getName(), p0, p1 - p0, p2 - p0), entity);
			InfoByEntity& infoByEntity = m_infoByEntityArray[entity].emplace_back(shapeId);
			infoByEntity.shapeType = shapeType;

			glm::vec2 scale;
			glm::vec3 rotation;
			glm::vec3 offset;
			computeRectangleScaleRotationOffsetFromPoints(shapeAsRectangle.getP0(), shapeAsRectangle.getS1(), shapeAsRectangle.getS2(), scale, rotation, offset);

			glm::mat4 transform;
			computeTransform(glm::vec3(scale, 1.0f), rotation, offset, transform);

			infoByEntity.rectangleTransform = physicsMeshInfo.transform * transform;
		}
		else if (shapeType == Wolf::Physics::PhysicsShapeType::Box)
		{
			const Wolf::Physics::Box& shapeAsBox = dynamic_cast<const Wolf::Physics::Box&>(*physicsMeshInfo.shape);

			glm::vec3 scale;
			glm::quat rotationQuat;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(physicsMeshInfo.transform, scale, rotationQuat, translation, skew, perspective);

			glm::mat4 noRotationTransform(1.0f);
			noRotationTransform = glm::translate(noRotationTransform, translation);
			noRotationTransform = glm::scale(noRotationTransform, scale);

			glm::vec3 aabbMin = noRotationTransform * glm::vec4(shapeAsBox.getAABBMin(), 1.0f);
			glm::vec3 aabbMax = noRotationTransform * glm::vec4(shapeAsBox.getAABBMax(), 1.0f);
			glm::vec3 rotation = shapeAsBox.getRotation() + glm::eulerAngles(rotationQuat);

			Wolf::Physics::PhysicsManager::DynamicShapeId shapeId = m_physicsManager->addDynamicBox(Wolf::Physics::Box(shapeAsBox.getName(), aabbMin, aabbMax, rotation), entity);
			InfoByEntity& infoByEntity = m_infoByEntityArray[entity].emplace_back(shapeId);
			infoByEntity.shapeType = shapeType;

			infoByEntity.boxAABBMin = aabbMin;
			infoByEntity.boxAABBMax = aabbMax;
			infoByEntity.boxRotation = rotation;
		}
		else
		{
			Wolf::Debug::sendError("Unhandled shape type");
		}
	}

	m_meshMutex.unlock();
}

void EditorPhysicsManager::removeMeshesForEntity(Entity* entity)
{
	m_meshMutex.lock();

	if (m_infoByEntityArray.contains(entity))
	{
		const std::vector<InfoByEntity>& infoForEntity = m_infoByEntityArray[entity];

		for (const InfoByEntity& info : infoForEntity)
		{
			m_physicsManager->removeDynamicShape(info.shapeId);
		}

		m_infoByEntityArray[entity].clear();
	}

	m_meshMutex.unlock();
}

void EditorPhysicsManager::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	for (auto const& [entity, infoForEntity] : m_infoByEntityArray)
	{
		for (const InfoByEntity& info : infoForEntity)
		{
			if (info.shapeType == Wolf::Physics::PhysicsShapeType::Rectangle)
			{
				debugRenderingManager.addRectangle(info.rectangleTransform);
			}
			else if (info.shapeType == Wolf::Physics::PhysicsShapeType::Box)
			{
				Wolf::AABB aabb(info.boxAABBMin, info.boxAABBMax);
				debugRenderingManager.addBox(aabb, info.boxRotation);
			}
			else
			{
				Wolf::Debug::sendError("Unhandled shape type");
			}
		}
	}
}
