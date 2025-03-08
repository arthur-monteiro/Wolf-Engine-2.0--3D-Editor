#pragma once

#include <array>
#include <functional>
#include <string>

#include "AnimatedModel.h"
#include "ComponentInterface.h"
#include "ContaminationEmitter.h"
#include "ContaminationReceiver.h"
#include "EditorConfiguration.h"
#include "EntityContainer.h"
#include "GasCylinderComponent.h"
#include "MaterialComponent.h"
#include "TextureSetComponent.h"
#include "Particle.h"
#include "ParticleEmitter.h"
#include "PlayerComponent.h"
#include "PointLight.h"
#include "ResourceManager.h"
#include "SkyLight.h"
#include "StaticModel.h"

class RenderingPipelineInterface;

class ComponentInstancier
{
public:
	ComponentInstancier(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
		std::function<void(ComponentInterface*)> requestReloadCallback, std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback, 
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const Wolf::ResourceNonOwner<ResourceManager>& resourceManager, const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager,
		const Wolf::ResourceNonOwner<EntityContainer>& entityContainer);

	ComponentInterface* instanciateComponent(const std::string& componentId) const;

	std::string getAllComponentTypes(const Wolf::ResourceNonOwner<Entity>& selectedEntity) const;

private:
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	Wolf::ResourceNonOwner<ResourceManager> m_resourceManager;
	Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager> m_physicsManager;
	Wolf::ResourceNonOwner<EntityContainer> m_entityContainer;

	struct ComponentInfo
	{
		std::string name;
		std::string id;
		std::function<ComponentInterface*()> instancingFunction;
	};

	std::array<ComponentInfo, 12> m_componentsInfo =
	{
		ComponentInfo
		{
			"Static model",
			StaticModel::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new StaticModel(m_materialsGPUManager, m_resourceManager, m_requestReloadCallback, m_getEntityFromLoadingPathCallback));
			}
		},
		ComponentInfo
		{
			"Contamination emitter",
			ContaminationEmitter::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new ContaminationEmitter(m_renderingPipeline, m_requestReloadCallback, m_materialsGPUManager, m_editorConfiguration, m_getEntityFromLoadingPathCallback, m_physicsManager));
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
				return static_cast<ComponentInterface*>(new PlayerComponent(m_getEntityFromLoadingPathCallback, m_entityContainer));
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
		},
		ComponentInfo
		{
			"Particle emitter",
			ParticleEmitter::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new ParticleEmitter(m_renderingPipeline, m_getEntityFromLoadingPathCallback, m_requestReloadCallback));
			}
		},
		ComponentInfo
		{
			"Particle component",
			Particle::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new Particle(m_materialsGPUManager, m_editorConfiguration, m_getEntityFromLoadingPathCallback));
			}
		},
		ComponentInfo
		{
			"Sky light",
			SkyLight::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new SkyLight());
			}
		},
		ComponentInfo
		{
			"Texture set",
			TextureSetComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new TextureSetComponent(m_materialsGPUManager, m_editorConfiguration, m_requestReloadCallback));
			}
		},
		ComponentInfo
		{
			"Material",
			MaterialComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new MaterialComponent(m_materialsGPUManager, m_requestReloadCallback, m_getEntityFromLoadingPathCallback));
			}
		},
		ComponentInfo
		{
			"Animated model",
			AnimatedModel::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new AnimatedModel(m_materialsGPUManager, m_resourceManager, m_getEntityFromLoadingPathCallback, m_renderingPipeline, m_requestReloadCallback));
			}
		},
		ComponentInfo
		{
			"Gas Cylinder",
			GasCylinderComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new GasCylinderComponent(m_physicsManager));
			}
		}
	};
};

