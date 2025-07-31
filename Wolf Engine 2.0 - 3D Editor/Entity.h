#pragma once

#include <array>

#include <AABB.h>
#include <DynamicStableArray.h>
#include <DynamicResourceUniqueOwnerArray.h>

#include "BoundingSphere.h"
#include "ComponentInterface.h"
#include "DrawManager.h"
#include "EditorLightInterface.h"
#include "EditorTypes.h"
#include "GameContext.h"
#include "LightManager.h"
#include "Notifier.h"
#include "RayTracedWorldManager.h"

class EditorPhysicsManager;
class EditorConfiguration;
class ComponentInstancier;

class Entity : public Notifier
{
public:
	Entity(std::string filePath, const std::function<void(Entity*)>&& onChangeCallback);
	virtual ~Entity() = default;
	void loadParams(const std::function<ComponentInterface* (const std::string&)>& instanciateComponent);

	void addComponent(ComponentInterface* component);
	void removeAllComponents();
	void setIdx(uint32_t idx) { m_idx = idx; }

	virtual void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager, const Wolf::ResourceNonOwner<EditorPhysicsManager>& editorPhysicsManager);
	void addLightToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const;
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) const;
	bool getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos);
	virtual void activateParams() const;
	virtual void fillJSONForParams(std::string& outJSON);

	virtual void save() const;

	virtual const std::string& getName() const { return m_nameParam; }
	const std::string& getLoadingPath() const { return m_filepath; }
	virtual std::string computeEscapedLoadingPath() const;
	virtual bool isFake() const { return false; }
	uint32_t getIdx() const { return m_idx; }

	void setIncludeEntityParams(bool value) { m_includeEntityParams = value; }

	Wolf::DynamicStableArray<Wolf::ResourceUniqueOwner<ComponentInterface>, 8>& getAllComponents() { return m_components; }

	Wolf::AABB getAABB() const;
	Wolf::BoundingSphere getBoundingSphere() const;
	bool hasModelComponent() const { return m_modelComponent.get(); }
	bool hasComponent(const std::string& componentId) const;
	glm::vec3 getPosition() const;
	void setPosition(const glm::vec3& newPosition) const;
	void setRotation(const glm::vec3& newRotation) const;

	template <typename T>
	[[nodiscard]] Wolf::NullableResourceNonOwner<T> getComponent()
	{
		for (uint32_t i = 0; i < m_components.size(); ++i)
		{
			if (const Wolf::ResourceNonOwner<T> componentAsRequestedType = m_components[i].createNonOwnerResource<T>())
			{
				return componentAsRequestedType;
			}
		}

		return Wolf::NullableResourceNonOwner<T>(); // will be nullptr here
	}

	template <typename T>
	[[nodiscard]] T* releaseComponent()
	{
		for (uint32_t i = 0; i < m_components.size(); ++i)
		{
			if (const Wolf::ResourceNonOwner<T> componentAsRequestedType = m_components[i].createNonOwnerResource<T>())
			{
				return static_cast<T*>(m_components[i].release());
			}
		}

		return nullptr;
	}

	void setName(const std::string& name) { m_nameParam = name; }

private:
	std::string m_filepath;
	std::function<void(Entity*)> m_onChangeCallback;

	static constexpr uint32_t MAX_COMPONENT_COUNT = 8;
	Wolf::DynamicResourceUniqueOwnerArray<ComponentInterface> m_components;

	// Model related
	std::unique_ptr<Wolf::ResourceNonOwner<EditorModelInterface>> m_modelComponent;
	bool m_needsMeshesToRenderComputation = false;
	bool m_needsMeshesForPhysicsComputation = false;

	// Light related
	std::vector<Wolf::ResourceNonOwner<EditorLightInterface>> m_lightComponents;

	EditorParamString m_nameParam = EditorParamString("Name", "Entity", "General", [this]() { m_onChangeCallback(this); });
	std::array<EditorParamInterface*, 1> m_entityParams =
	{
		&m_nameParam
	};

	bool m_includeEntityParams = true;

	static constexpr uint32_t INVALID_ID = -1;
	uint32_t m_idx = INVALID_ID;
};
