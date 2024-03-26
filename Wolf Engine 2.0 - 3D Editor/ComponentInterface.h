#pragma once

#include <string>

#include "RenderMeshList.h"

namespace Wolf
{
	class JSONReader;
}

class DebugRenderingManager;
class Entity;

class ComponentInterface
{
public:
	virtual ~ComponentInterface() = default;

	virtual void loadParams(Wolf::JSONReader& jsonReader) = 0;

	virtual void activateParams() = 0;
	virtual void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) = 0;
	
	virtual std::string getId() const = 0;

	virtual void updateBeforeFrame() = 0;
	virtual void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) = 0;
	virtual void addDebugInfo(DebugRenderingManager& debugRenderingManager) = 0;

protected:
	ComponentInterface() = default;
};

