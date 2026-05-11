#pragma once

#include <array>
#include <functional>
#include <string>

#include "AnimatedMesh.h"
#include "AssetManager.h"
#include "CameraSettingsComponent.h"
#include "ColorGradingComponent.h"
#include "ComponentInterface.h"
#include "ContaminationEmitter.h"
#include "ContaminationMaterial.h"
#include "ContaminationReceiver.h"
#include "EditorConfiguration.h"
#include "EntityContainer.h"
#include "ExternalSceneComponent.h"
#include "GasCylinderComponent.h"
#include "ParticleEmitter.h"
#include "PlayerComponent.h"
#include "PointLight.h"
#include "SkyLight.h"
#include "StaticMesh.h"
#include "SurfaceCoatingEmitterComponent.h"

class RenderingPipelineInterface;

class ComponentInstancier
{
public:
	ComponentInstancier(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
		std::function<void(ComponentInterface*)> requestReloadCallback, const std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback,
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const Wolf::ResourceNonOwner<AssetManager>& assetManager, const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager,
		const Wolf::ResourceNonOwner<EntityContainer>& entityContainer, const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface,
		const std::function<Entity*(ComponentInterface*, const std::string&)>& createEntityCallback);

	ComponentInterface* instanciateComponent(const std::string& componentId) const;

	std::string getAllComponentTypes(const Wolf::ResourceNonOwner<Entity>& selectedEntity) const;

private:
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	Wolf::ResourceNonOwner<AssetManager> m_assetManager;
	Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager> m_physicsManager;
	Wolf::ResourceNonOwner<EntityContainer> m_entityContainer;
	Wolf::ResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;
	std::function<Entity*(ComponentInterface*, const std::string&)> m_createEntityCallback;

	struct ComponentInfo
	{
		std::string name;
		std::string id;
		std::function<ComponentInterface*()> instancingFunction;
	};

	std::array<ComponentInfo, 14> m_componentsInfo =
	{
		ComponentInfo
		{
			"Static model",
			StaticMesh::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new StaticMesh(m_assetManager));
			}
		},
		ComponentInfo
		{
			"Contamination emitter",
			ContaminationEmitter::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new ContaminationEmitter(m_renderingPipeline, m_requestReloadCallback, m_materialsGPUManager, m_editorConfiguration, m_getEntityFromLoadingPathCallback,
					m_physicsManager, m_bufferPoolInterface));
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
				return static_cast<ComponentInterface*>(new PlayerComponent(m_getEntityFromLoadingPathCallback, m_entityContainer, m_renderingPipeline, m_bufferPoolInterface));
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
				return static_cast<ComponentInterface*>(new ParticleEmitter(m_renderingPipeline, m_assetManager));
			}
		},
		ComponentInfo
		{
			"Sky light",
			SkyLight::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new SkyLight(m_requestReloadCallback, m_assetManager, m_renderingPipeline, m_bufferPoolInterface));
			}
		},
		ComponentInfo
		{
			"Animated model",
			AnimatedMesh::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new AnimatedMesh(m_assetManager, m_getEntityFromLoadingPathCallback, m_renderingPipeline, m_requestReloadCallback));
			}
		},
		ComponentInfo
		{
			"Gas cylinder",
			GasCylinderComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new GasCylinderComponent(m_physicsManager, m_getEntityFromLoadingPathCallback, m_requestReloadCallback));
			}
		},
		ComponentInfo
		{
			"Contamination material",
			ContaminationMaterial::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new ContaminationMaterial(m_getEntityFromLoadingPathCallback));
			}
		},
		ComponentInfo
		{
			"Color grading",
			ColorGradingComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new ColorGradingComponent(m_assetManager, m_renderingPipeline));
			}
		},
		ComponentInfo
		{
			"Camera settings",
			CameraSettingsComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new CameraSettingsComponent(m_renderingPipeline));
			}
		},
		ComponentInfo
		{
			"Surface coating emitter",
			SurfaceCoatingEmitterComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new SurfaceCoatingEmitterComponent(m_renderingPipeline, m_assetManager, m_requestReloadCallback, m_getEntityFromLoadingPathCallback));
			}
		},
		ComponentInfo
		{
			"External scene",
			ExternalSceneComponent::ID,
			[this]()
			{
				return static_cast<ComponentInterface*>(new ExternalSceneComponent(m_assetManager, m_getEntityFromLoadingPathCallback, m_createEntityCallback, m_materialsGPUManager,
					m_requestReloadCallback, m_renderingPipeline));
			}
		}
	};
};

