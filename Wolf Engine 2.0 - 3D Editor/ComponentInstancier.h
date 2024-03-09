#pragma once

#include <array>
#include <functional>
#include <string>

#include <AppCore/JSHelpers.h>

#include "BuildingModel.h"
#include "ComponentInterface.h"
#include "StaticModel.h"

class ComponentInstancier
{
public:
	ComponentInterface* instanciateComponent(const std::string& componentId, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager);

	std::string getAllComponentTypes() const;

private:
	struct ComponentInfo
	{
		std::string name;
		std::string id;
		std::function<ComponentInterface*(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& bindlessDescriptor) > instancingFunction;
	};

	std::array<ComponentInfo, 2> m_componentsInfo =
	{
		ComponentInfo
		{
			"Static model",
			StaticModel::ID,
			[](const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager)
			{
				return static_cast<ComponentInterface*>(new StaticModel(glm::mat4(1.0f), materialsGPUManager));
			}
		},
		ComponentInfo
		{
			"Building model",
			BuildingModel::ID,
			[](const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager)
			{
				return static_cast<ComponentInterface*>(new BuildingModel(glm::mat4(1.0f), materialsGPUManager));
			}
		}
	};
};

