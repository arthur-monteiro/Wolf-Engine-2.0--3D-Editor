#pragma once

#include <array>

#include "AABB.h"
#include "ComponentInterface.h"
#include "EditorTypes.h"
#include "GameContext.h"
#include "ResourceUniqueOwner.h"

class EditorConfiguration;
class ComponentInstancier;

class Entity
{
public:
	Entity(const std::string& filePath, const std::function<void(Entity*)>&& onChangeCallback, const std::function<ComponentInterface*(const std::string&)>&& instanciateComponent);

	void addComponent(ComponentInterface* component);

	const std::string& getName() const { return m_nameParam; }
	const std::string& getLoadingPath() const { return m_filepath; }

	std::vector<Wolf::ResourceUniqueOwner<ComponentInterface>>& getAllComponents() { return m_components; }
	bool hasModelComponent() const { return m_modelComponent.get(); }

	void activateParams() const;
	void fillJSONForParams(std::string& outJSON);

	void save();

	void removeAllComponents();

	Wolf::AABB getAABB() const;

private:
	std::string m_filepath;
	std::function<void(Entity*)> m_onChangeCallback;

	static constexpr uint32_t MAX_COMPONENT_COUNT = 8;
	std::vector<Wolf::ResourceUniqueOwner<ComponentInterface>> m_components;
	std::unique_ptr<Wolf::ResourceNonOwner<EditorModelInterface>> m_modelComponent;

	EditorParamString m_nameParam = EditorParamString("Name", "Entity", "General", [this]() { m_onChangeCallback(this); });
	std::array<EditorParamInterface*, 1> m_entityParams =
	{
		&m_nameParam
	};
};
