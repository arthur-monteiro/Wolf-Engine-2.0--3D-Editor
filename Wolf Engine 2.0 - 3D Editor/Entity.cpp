#include "Entity.h"

#include <fstream>

#include "ComponentInstancier.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"

Entity::Entity(const std::string& filePath, const std::function<void(Entity*)>&& onChangeCallback, const std::function<ComponentInterface* (const std::string&)>&& instanciateComponent) : m_filepath(filePath), m_onChangeCallback(onChangeCallback)
{
	m_nameParam = "Undefined";
	m_components.reserve(MAX_COMPONENT_COUNT);

	const std::ifstream inFile(g_editorConfiguration->computeFullPathFromLocalPath(filePath));
	if (inFile.good())
	{
		Wolf::JSONReader jsonReader(g_editorConfiguration->computeFullPathFromLocalPath(filePath));
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
	if (m_components.size() > MAX_COMPONENT_COUNT)
	{
		Wolf::Debug::sendCriticalError("There are more components than supported. All unique owner pointers has been changed resulting in garbage references for non owners");
	}

	if (const Wolf::ResourceNonOwner<EditorModelInterface> componentAsModel = m_components.back().createNonOwnerResource<EditorModelInterface>())
	{
		if (hasModelComponent())
			Wolf::Debug::sendError("Adding a second model component to the entity " + std::string(m_nameParam));
		m_modelComponent.reset(new Wolf::ResourceNonOwner<EditorModelInterface>(componentAsModel));
	}
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
	if (const size_t commaPos = outJSON.substr(outJSON.size() - 3).find(','); commaPos != std::string::npos)
	{
		outJSON.erase(commaPos + outJSON.size() - 3);
	}
	outJSON += "\t]\n";
	outJSON += "}";
}

void Entity::save()
{
	std::ofstream outFile(g_editorConfiguration->computeFullPathFromLocalPath(m_filepath));

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

	for (uint32_t i = 0; i < m_components.size(); ++i)
	{
		const Wolf::ResourceUniqueOwner<ComponentInterface>& component = m_components[i];

		outJSON += "\t" R"(")" + component->getId() + R"(": {)" "\n";
		outJSON += "\t\t" R"("params": [)" "\n";
		component->addParamsToJSON(outJSON, 3);
		if (const size_t commaPos = outJSON.substr(outJSON.size() - 3).find(','); commaPos != std::string::npos)
		{
			outJSON.erase(commaPos + outJSON.size() - 3);
		}
		outJSON += "\t\t]\n";
		outJSON += "\t}";
		outJSON += i == m_components.size() - 1 ? "\n" : ",\n";
	}

	outJSON += "}";

	outFile << outJSON;
}

void Entity::removeAllComponents()
{
	m_modelComponent.reset();
	m_components.clear();
}

Wolf::AABB Entity::getAABB() const
{
	if (m_modelComponent)
	{
		return (*m_modelComponent)->getAABB();
	}
	Wolf::Debug::sendWarning("Getting AABB of entity which doesn't contain a model component");
	return {};
}
