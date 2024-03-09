#include "ComponentInstancier.h"

ComponentInterface* ComponentInstancier::instanciateComponent(const std::string& componentId, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager)
{
	for (const ComponentInfo& componentInfo : m_componentsInfo)
	{
		if (componentInfo.id == componentId)
			return componentInfo.instancingFunction(materialsGPUManager);
	}

	return nullptr;
}

std::string ComponentInstancier::getAllComponentTypes() const
{
	std::string r = R"({ "components": [)";
	for (uint32_t i = 0; i < m_componentsInfo.size(); ++i)
	{
		const ComponentInfo& componentInfo = m_componentsInfo[i];

		r += R"({ "name":")" + componentInfo.name + R"(", "value":")" + componentInfo.id + R"(" })";
		if (i != m_componentsInfo.size() - 1)
			r += ", ";
	}
	r += R"( ] })";

	return r;
}
