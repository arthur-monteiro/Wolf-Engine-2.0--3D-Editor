#pragma once

#include <array>

#include <AABB.h>
#include <DynamicStableArray.h>
#include <DynamicResourceUniqueOwnerArray.h>

#include "ComponentInterface.h"
#include "EditorLightInterface.h"
#include "EditorTypes.h"
#include "GameContext.h"
#include "LightManager.h"

class EditorConfiguration;
class ComponentInstancier;

class Entity
{
public:
	Entity(std::string filePath, const std::function<void(Entity*)>&& onChangeCallback);
	virtual ~Entity() = default;
	void loadParams(const std::function<ComponentInterface* (const std::string&)>& instanciateComponent);

	void addComponent(ComponentInterface* component);
	void removeAllComponents();

	virtual void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer);
	void addMeshesToRenderList(Wolf::RenderMeshList& renderMeshList) const;
	void addLightToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const;
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) const;
	virtual void activateParams() const;
	virtual void fillJSONForParams(std::string& outJSON);

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) const;

	virtual void save() const;

	virtual const std::string& getName() const { return m_nameParam; }
	const std::string& getLoadingPath() const { return m_filepath; }
	virtual std::string computeEscapedLoadingPath() const;
	virtual bool isFake() const { return false; }

	Wolf::DynamicStableArray<Wolf::ResourceUniqueOwner<ComponentInterface>, 8>& getAllComponents() { return m_components; }

	Wolf::AABB getAABB() const;
	bool hasModelComponent() const { return m_modelComponent.get(); }
	bool hasComponent(const std::string& componentId) const;
	glm::vec3 getPosition() const;
	void setPosition(const glm::vec3& newPosition) const;

	template <typename T>
	Wolf::ResourceNonOwner<T> getComponent()
	{
		for (uint32_t i = 0; i < m_components.size(); ++i)
		{
			if (const Wolf::ResourceNonOwner<T> componentAsRequestedType = m_components[i].createNonOwnerResource<T>())
			{
				return componentAsRequestedType;
			}
		}

		return m_components[0].createNonOwnerResource<T>();
	}

	void setName(const std::string& name) { m_nameParam = name; }

private:
	std::string m_filepath;
	std::function<void(Entity*)> m_onChangeCallback;

	static constexpr uint32_t MAX_COMPONENT_COUNT = 8;
	Wolf::DynamicResourceUniqueOwnerArray<ComponentInterface> m_components;
	std::unique_ptr<Wolf::ResourceNonOwner<EditorModelInterface>> m_modelComponent;
	std::vector<Wolf::ResourceNonOwner<EditorLightInterface>> m_lightComponents;
	bool m_requiresInputs = false;

	EditorParamString m_nameParam = EditorParamString("Name", "Entity", "General", [this]() { m_onChangeCallback(this); });
	std::array<EditorParamInterface*, 1> m_entityParams =
	{
		&m_nameParam
	};
};
