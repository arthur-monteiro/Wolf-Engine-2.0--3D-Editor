#include "Entity.h"

#include "ComponentInstancier.h"
#include "EditorParamsHelper.h"

Entity::Entity(const std::string& filepath, const std::function<void(Entity*)>&& onChangeCallback, const std::function<ComponentInterface* (const std::string&)>&& instanciateComponent) : m_filepath(filepath), m_onChangeCallback(onChangeCallback)
{
	m_nameParam = "Undefined";

	const std::ifstream inFile(filepath.c_str());
	if (inFile.good())
	{
		Wolf::JSONReader jsonReader(filepath);
		loadParams(jsonReader, "entity", m_entityParams);

		const uint32_t componentCount = jsonReader.getRoot()->getPropertyCount();
		for (uint32_t i = 0; i < componentCount; ++i)
		{
			std::string componentId = jsonReader.getRoot()->getPropertyString(i);
			if (componentId == "entity")
				continue;
			ComponentInterface* component = instanciateComponent(componentId);
			component->loadParams(jsonReader);
			addComponent(component);
		}
	}
}

void Entity::addComponent(ComponentInterface* component)
{
	m_components.emplace_back(component);
}

void Entity::activateParams() const
{
	for (EditorParamInterface* param : m_entityParams)
	{
		param->activate();
	}

	for (const Wolf::ResourceUniqueOwner<ComponentInterface>& component : m_components)
	{
		component->activateParams();
	}
}

void Entity::fillJSONForParams(std::string& outJSON)
{
	outJSON += "{\n";
	outJSON += "\t" R"("params": [)" "\n";
	addParamsToJSON(outJSON, m_entityParams, m_components.empty());
	for (const Wolf::ResourceUniqueOwner<ComponentInterface>& component : m_components)
	{
		component->addParamsToJSON(outJSON);
	}
	outJSON += "\t]\n";
	outJSON += "}";
}

void Entity::save()
{
	std::ofstream outFile(m_filepath);

	std::string outJSON;
	outJSON += "{\n";

	outJSON += "\t" R"("entity": {)" "\n";
	outJSON += "\t\t" R"("params": [)" "\n";
	addParamsToJSON(outJSON, m_entityParams, true, 3);
	outJSON += "\t\t]\n";
	outJSON += "\t}";
	if (!m_components.empty())
		outJSON += ",";
	outJSON += "\n";

	for (const Wolf::ResourceUniqueOwner<ComponentInterface>& component : m_components)
	{
		outJSON += "\t" R"(")" + component->getId() + R"(": {)" "\n";
		outJSON += "\t\t" R"("params": [)" "\n";
		component->addParamsToJSON(outJSON, 3);
		outJSON += "\t\t]\n";
		outJSON += "\t}";
	}

	outJSON += "}";

	outFile << outJSON;
}

void Entity::removeAllComponents()
{
	m_components.clear();
}
