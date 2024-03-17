#include "ComponentInstancier.h"

#include "Entity.h"

ComponentInterface* ComponentInstancier::instanciateComponent(const std::string& componentId, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const std::function<void(ComponentInterface*)>& requestReloadCallback)
{
	for (const ComponentInfo& componentInfo : m_componentsInfo)
	{
		if (componentInfo.id == componentId)
			return componentInfo.instancingFunction(materialsGPUManager, requestReloadCallback);
	}

	return nullptr;
}

std::string ComponentInstancier::getAllComponentTypes(const Wolf::ResourceNonOwner<Entity>& selectedEntity) const
{
	std::string r = R"({ "components": [)";
	for (uint32_t i = 0; i < m_componentsInfo.size(); ++i)
	{
		const ComponentInfo& componentInfo = m_componentsInfo[i];

		// Model interface check
		if ((componentInfo.id == StaticModel::ID || componentInfo.id == BuildingModel::ID) && selectedEntity->hasModelComponent())
			continue;

		r += R"({ "name":")" + componentInfo.name + R"(", "value":")" + componentInfo.id + R"(" })";
		if (i != m_componentsInfo.size() - 1)
			r += ", ";
	}
	r += R"( ] })";

	return r;
}
