#pragma once

#include <array>

#include "AABB.h"
#include "ComponentInterface.h"
#include "EditorTypes.h"
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

	void activateParams() const;
	void fillJSONForParams(std::string& outJSON);

	void save();

	void removeAllComponents();

	Wolf::AABB getAABB();

private:
	std::string m_filepath;
	std::function<void(Entity*)> m_onChangeCallback;
	std::vector<Wolf::ResourceUniqueOwner<ComponentInterface>> m_components;

	EditorParamString m_nameParam = EditorParamString("Name", "Entity", "General", [this]() { m_onChangeCallback(this); });
	std::array<EditorParamInterface*, 1> m_entityParams =
	{
		&m_nameParam
	};
};

