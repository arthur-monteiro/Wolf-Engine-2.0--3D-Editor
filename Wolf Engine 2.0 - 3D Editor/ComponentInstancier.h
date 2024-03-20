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
	ComponentInstancier(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, std::function<void(ComponentInterface*)> requestReloadCallback, 
		std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback);

	ComponentInterface* instanciateComponent(const std::string& componentId) const;

	std::string getAllComponentTypes(const Wolf::ResourceNonOwner<Entity>& selectedEntity) const;

private:
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	struct ComponentInfo
	{
		std::string name;
		std::string id;
		std::function<ComponentInterface*()> instancingFunction;
	};

	std::array<ComponentInfo, 4> m_componentsInfo =
	{
		ComponentInfo
		{
			"Static model",
			StaticModel::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new StaticModel(glm::mat4(1.0f), m_materialsGPUManager));
			}
		},
		ComponentInfo
		{
			"Building model",
			BuildingModel::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new BuildingModel(glm::mat4(1.0f), m_materialsGPUManager));
			}
		},
		ComponentInfo
		{
			"Contamination emitter",
			ContaminationEmitter::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new ContaminationEmitter(m_requestReloadCallback));
			}
		},
		ComponentInfo
		{
			"Contamination receiver",
			ContaminationReceiver::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new ContaminationReceiver(m_getEntityFromLoadingPathCallback));
			}
		}
	};
};

