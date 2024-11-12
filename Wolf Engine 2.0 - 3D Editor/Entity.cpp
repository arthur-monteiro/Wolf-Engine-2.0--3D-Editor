#include "Entity.h"

#include <fstream>

#include <ProfilerCommon.h>

#include "ComponentInstancier.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"

Entity::Entity(std::string filePath, const std::function<void(Entity*)>&& onChangeCallback) : m_filepath(std::move(filePath)), m_onChangeCallback(onChangeCallback)
{
	m_nameParam = "Undefined";
}

void Entity::loadParams(const std::function<ComponentInterface* (const std::string&)>& instanciateComponent)
{
	const std::ifstream inFile(g_editorConfiguration->computeFullPathFromLocalPath(m_filepath));
	if (inFile.good())
	{
		Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { g_editorConfiguration->computeFullPathFromLocalPath(m_filepath) });

		if (static_cast<const std::string&>(m_nameParam) == "Undefined")
			::loadParams(jsonReader, "entity", m_entityParams);

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
	component->registerEntity(this);

	m_components.emplace_back(component);
	if (m_components.size() > MAX_COMPONENT_COUNT)
	{
		Wolf::Debug::sendError("There are more components than supported, please remove components");
	}

	if (const Wolf::ResourceNonOwner<EditorModelInterface> componentAsModel = m_components.back().createNonOwnerResource<EditorModelInterface>())
	{
		if (hasModelComponent())
			Wolf::Debug::sendError("Adding a second model component to the entity " + std::string(m_nameParam));
		m_modelComponent.reset(new Wolf::ResourceNonOwner<EditorModelInterface>(componentAsModel));
		(*m_modelComponent)->subscribe(this, [this]() { m_needsMeshesToRenderComputation = true; });
	}
	else if (const Wolf::ResourceNonOwner<EditorLightInterface> componentAsLight = m_components.back().createNonOwnerResource<EditorLightInterface>())
	{
		m_lightComponents.push_back(componentAsLight);
	}

	if (m_components.back()->requiresInputs())
		m_requiresInputs = true;

	if (m_modelComponent)
		m_needsMeshesToRenderComputation = true;
}

std::string Entity::computeEscapedLoadingPath() const
{
	std::string escapedLoadingPath;
	for (const char character : getLoadingPath())
	{
		if (character == '\\')
			escapedLoadingPath += "\\\\";
		else
			escapedLoadingPath += character;
	}

	return escapedLoadingPath;
}

void Entity::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager)
{
	PROFILE_FUNCTION

	if (m_requiresInputs)
	{
		inputHandler->lockCache(this);
		inputHandler->pushDataToCache(this);
		inputHandler->unlockCache(this);
	}

	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->updateBeforeFrame(globalTimer);)

	if (m_needsMeshesToRenderComputation)
	{
		std::vector<DrawManager::DrawMeshInfo> meshes;
		(*m_modelComponent)->getMeshesToRender(meshes);

		DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->alterMeshesToRender(meshes);)

		// Re-request meshes until they are loaded
		if (!meshes.empty())
		{
			m_needsMeshesToRenderComputation = false;
			drawManager->addMeshesToDraw(meshes, this);
		}
	}
}

void Entity::addLightToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const
{
	for (const Wolf::ResourceNonOwner<EditorLightInterface>& lightComponent : m_lightComponents)
	{
		lightComponent->addLightsToLightManager(lightManager);
	}
}

void Entity::addDebugInfo(DebugRenderingManager& debugRenderingManager) const
{
	PROFILE_FUNCTION

	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->addDebugInfo(debugRenderingManager);)
}

void Entity::activateParams() const
{
	for (EditorParamInterface* param : m_entityParams)
	{
		param->activate();
	}

	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->activateParams();)
}

void Entity::fillJSONForParams(std::string& outJSON)
{
	outJSON += "{\n";
	outJSON += "\t" R"("params": [)" "\n";
	addParamsToJSON(outJSON, m_entityParams, m_components.empty());
	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_CONST_RANGE_LOOP(m_components, component, component->addParamsToJSON(outJSON);)
	if (const size_t commaPos = outJSON.substr(outJSON.size() - 3).find(','); commaPos != std::string::npos)
	{
		outJSON.erase(commaPos + outJSON.size() - 3);
	}
	outJSON += "\t]\n";
	outJSON += "}";
}

void Entity::updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) const
{
	PROFILE_FUNCTION

	if (m_requiresInputs)
		inputHandler->lockCache(this);

	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->updateDuringFrame(inputHandler);)

	if (m_requiresInputs)
	{
		inputHandler->clearCache(this);
		inputHandler->unlockCache(this);
	}
}

void Entity::save() const
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

	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component,
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
		)

	outJSON += "}";

	outFile << outJSON;
}

void Entity::removeAllComponents()
{
	m_modelComponent.reset();
	m_lightComponents.clear();
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

bool Entity::hasComponent(const std::string& componentId) const
{
	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component,
		if (component->getId() == componentId)
			return true;
		);
	return false;
}

glm::vec3 Entity::getPosition() const
{
	if (m_modelComponent)
		return (*m_modelComponent)->getPosition();

	Wolf::Debug::sendWarning("Getting position of entity which doesn't contain a model component");
	return glm::vec3(0.0);
}

void Entity::setPosition(const glm::vec3& newPosition) const
{
	if (m_modelComponent)
	{
		(*m_modelComponent)->setPosition(newPosition);
		return;
	}

	Wolf::Debug::sendWarning("Trying to set position of entity which doesn't contain a model component");
}
