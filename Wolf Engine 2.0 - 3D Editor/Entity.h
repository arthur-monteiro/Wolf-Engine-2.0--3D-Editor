#pragma once

#include <array>

#include <AABB.h>
#include <DynamicStableArray.h>
#include <DynamicResourceUniqueOwnerArray.h>

#include "ComponentInterface.h"
#include "EditorTypes.h"
#include "GameContext.h"

class EditorConfiguration;
class ComponentInstancier;

class Entity
{
public:
	Entity(std::string filePath, const std::function<void(Entity*)>&& onChangeCallback);
	void loadParams(const std::function<ComponentInterface* (const std::string&)>& instanciateComponent);

	void addComponent(ComponentInterface* component);
	void removeAllComponents();

	void updateBeforeFrame() const;
	void addMeshesToRenderList(Wolf::RenderMeshList& renderMeshList) const;
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) const;
	void activateParams() const;
	void fillJSONForParams(std::string& outJSON);

	void save();

	const std::string& getName() const { return m_nameParam; }
	const std::string& getLoadingPath() const { return m_filepath; }
	std::string computeEscapedLoadingPath() const;

	Wolf::DynamicStableArray<Wolf::ResourceUniqueOwner<ComponentInterface>, 8>& getAllComponents() { return m_components; }
	Wolf::AABB getAABB() const;
	bool hasModelComponent() const { return m_modelComponent.get(); }

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

private:
	std::string m_filepath;
	std::function<void(Entity*)> m_onChangeCallback;

	static constexpr uint32_t MAX_COMPONENT_COUNT = 8;
	Wolf::DynamicResourceUniqueOwnerArray<ComponentInterface> m_components;
	std::unique_ptr<Wolf::ResourceNonOwner<EditorModelInterface>> m_modelComponent;

	EditorParamString m_nameParam = EditorParamString("Name", "Entity", "General", [this]() { m_onChangeCallback(this); });
	std::array<EditorParamInterface*, 1> m_entityParams =
	{
		&m_nameParam
	};
};
