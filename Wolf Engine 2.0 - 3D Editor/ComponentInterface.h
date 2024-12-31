#pragma once

#include <string>

#include <Debug.h>
#include <RenderMeshList.h>
#include <Timer.h>

#include "DrawManager.h"

namespace Wolf
{
	class InputHandler;
	class JSONReader;
}

class DebugRenderingManager;
class Entity;

class ComponentInterface
{
public:
	virtual ~ComponentInterface() = default;

	virtual void loadParams(Wolf::JSONReader& jsonReader) = 0;
	void registerEntity(Entity* entity)
	{
		if (m_entity != nullptr)
			Wolf::Debug::sendCriticalError("Component is already associated to an entity");
		m_entity = entity;

		onEntityRegistered();
	}

	virtual void activateParams() = 0;
	virtual void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) = 0;
	
	virtual std::string getId() const = 0;

	virtual void updateBeforeFrame(const Wolf::Timer& globalTimer) = 0;
	virtual void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) = 0;
	virtual void addDebugInfo(DebugRenderingManager& debugRenderingManager) = 0;

	virtual void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) = 0;
	virtual bool requiresInputs() const { return false; }

protected:
	ComponentInterface() = default;
	virtual void onEntityRegistered() {}

	Entity* m_entity = nullptr;
};

