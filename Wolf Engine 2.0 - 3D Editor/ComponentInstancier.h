#pragma once

#include <array>
#include <functional>
#include <string>

#include <AppCore/JSHelpers.h>

#include "BuildingModel.h"
#include "ComponentInterface.h"
#include "ContaminationEmitter.h"
#include "ContaminationReceiver.h"
#include "StaticModel.h"

class ComponentInstancier
{
public:
	ComponentInterface* instanciateComponent(const std::string& componentId, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const std::function<void(ComponentInterface*)>& requestReloadCallback);

	std::string getAllComponentTypes(const Wolf::ResourceNonOwner<Entity>& selectedEntity) const;

private:
	struct ComponentInfo
	{
		std::string name;
		std::string id;
		std::function<ComponentInterface*(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const std::function<void(ComponentInterface*)>& requestReloadCallback)> instancingFunction;
	};

	std::array<ComponentInfo, 4> m_componentsInfo =
	{
		ComponentInfo
		{
			"Static model",
			StaticModel::ID,
			[](const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const std::function<void(ComponentInterface*)>& requestReloadCallback)
			{
				return static_cast<ComponentInterface*>(new StaticModel(glm::mat4(1.0f), materialsGPUManager));
			}
		},
		ComponentInfo
		{
			"Building model",
			BuildingModel::ID,
			[](const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const std::function<void(ComponentInterface*)>& requestReloadCallback)
			{
				return static_cast<ComponentInterface*>(new BuildingModel(glm::mat4(1.0f), materialsGPUManager));
			}
		},
		ComponentInfo
		{
			"Contamination emitter",
			ContaminationEmitter::ID,
			[](const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const std::function<void(ComponentInterface*)>& requestReloadCallback)
			{
				return static_cast<ComponentInterface*>(new ContaminationEmitter(requestReloadCallback));
			}
		},
		ComponentInfo
		{
			"Contamination receiver",
			ContaminationReceiver::ID,
			[](const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const std::function<void(ComponentInterface*)>& requestReloadCallback)
			{
				return static_cast<ComponentInterface*>(new ContaminationReceiver());
			}
		}
	};
};

