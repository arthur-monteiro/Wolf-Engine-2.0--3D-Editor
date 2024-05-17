#pragma once

#include <array>
#include <functional>
#include <string>

#include "BuildingModel.h"
#include "ComponentInterface.h"
#include "ContaminationEmitter.h"
#include "ContaminationReceiver.h"
#include "EditorConfiguration.h"
#include "PlayerComponent.h"
#include "PointLight.h"
#include "StaticModel.h"

class RenderingPipelineInterface;

class ComponentInstancier
{
public:
	ComponentInstancier(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
		std::function<void(ComponentInterface*)> requestReloadCallback, std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback, 
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

	ComponentInterface* instanciateComponent(const std::string& componentId) const;

	std::string getAllComponentTypes(const Wolf::ResourceNonOwner<Entity>& selectedEntity) const;

private:
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;

	struct ComponentInfo
	{
		std::string name;
		std::string id;
		std::function<ComponentInterface*()> instancingFunction;
	};

	std::array<ComponentInfo, 6> m_componentsInfo =
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
				return static_cast<ComponentInterface*>(new ContaminationEmitter(m_renderingPipeline, m_requestReloadCallback, m_materialsGPUManager, m_editorConfiguration));
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
		},
		ComponentInfo
		{
			"Player component",
			PlayerComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new PlayerComponent(m_getEntityFromLoadingPathCallback));
			}
		},
		ComponentInfo
		{
			"Point light",
			PointLight::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new PointLight());
			}
		}
	};
};

