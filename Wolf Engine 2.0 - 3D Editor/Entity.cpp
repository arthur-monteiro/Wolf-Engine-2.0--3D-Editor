#include "Entity.h"

#include <fstream>

#include <ProfilerCommon.h>

#include "ComponentInstancier.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"

Entity::Entity(std::string filePath, const std::function<void(Entity*)>&& onChangeCallback, const std::function<void(Entity*)>&& rebuildRayTracedWorldCallback)
	: m_filepath(std::move(filePath)), m_onChangeCallback(onChangeCallback), m_rebuildRayTracedWorldCallback(rebuildRayTracedWorldCallback)
{
	m_nameParam = "Undefined";
}

void Entity::loadParams(const std::function<ComponentInterface* (const std::string&)>& instanciateComponent)
{
	const std::ifstream inFile(g_editorConfiguration->computeFullPathFromLocalPath(m_filepath));
	if (!m_filepath.empty() && inFile.good())
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
	}
	else if (const Wolf::ResourceNonOwner<EditorLightInterface> componentAsLight = m_components.back().createNonOwnerResource<EditorLightInterface>())
	{
		m_lightComponents.push_back(componentAsLight);
	}

	if (m_modelComponent)
	{
		component->subscribe(this, [this](Flags) { m_needsMeshesToRenderComputation = m_needsMeshesForPhysicsComputation = true; });
		m_needsMeshesToRenderComputation = m_needsMeshesForPhysicsComputation = true;
	}

	notifySubscribers();
}

void Entity::releaseAllComponentNullableNonOwnerResources() const
{
	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->releaseAllNullableNonOwnerResources();)
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

void Entity::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager,
	const Wolf::ResourceNonOwner<EditorPhysicsManager>& editorPhysicsManager)
{
	PROFILE_FUNCTION

	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->updateBeforeFrame(globalTimer, inputHandler);)

	if (m_modelComponent)
	{
		if (m_needsMeshesToRenderComputation)
		{
			std::vector<DrawManager::DrawMeshInfo> meshes;
			bool areMeshesLoaded = (*m_modelComponent)->getMeshesToRender(meshes);

			DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->alterMeshesToRender(meshes);)

				// Re-request meshes until they are loaded
				if (areMeshesLoaded)
				{
					m_needsMeshesToRenderComputation = false;
					drawManager->addMeshesToDraw(meshes, this);

					m_rebuildRayTracedWorldCallback(this);
				}
				else
				{
					drawManager->removeMeshesForEntity(this);
				}
		}
		if (m_needsMeshesForPhysicsComputation)
		{
			std::vector<EditorPhysicsManager::PhysicsMeshInfo> meshes;

			// Re-request meshes until they are loaded
			if (bool areMeshesLoaded = (*m_modelComponent)->getMeshesForPhysics(meshes))
			{
				m_needsMeshesForPhysicsComputation = false;
				editorPhysicsManager->addMeshes(meshes, this);
			}
			else
			{
				editorPhysicsManager->removeMeshesForEntity(this);
			}
		}
	}
	else
	{
		m_needsMeshesToRenderComputation = m_needsMeshesForPhysicsComputation = false;
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

bool Entity::getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos)
{
	if (m_modelComponent)
	{
		return (*m_modelComponent)->getInstancesForRayTracedWorld(instanceInfos);
	}
	return true;
}

void Entity::activateParams()
{
	if (m_includeEntityParams)
	{
		for (EditorParamInterface* param : m_entityParams)
		{
			param->activate();
		}
	}

	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->activateParams();)
}

void Entity::fillJSONForParams(std::string& outJSON)
{
	outJSON += "{\n";
	outJSON += "\t" R"("params": [)" "\n";
	if (m_includeEntityParams)
	{
		addParamsToJSON(outJSON, m_entityParams, m_components.empty());
	}
	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_CONST_RANGE_LOOP(m_components, component, component->addParamsToJSON(outJSON);)
	if (const size_t commaPos = outJSON.substr(outJSON.size() - 3).find(','); commaPos != std::string::npos)
	{
		outJSON.erase(commaPos + outJSON.size() - 3);
	}
	outJSON += "\t]\n";
	outJSON += "}";
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

	DYNAMIC_RESOURCE_UNIQUE_OWNER_ARRAY_RANGE_LOOP(m_components, component, component->saveCustom();)
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

Wolf::BoundingSphere Entity::getBoundingSphere() const
{
	if (m_modelComponent)
	{
		return (*m_modelComponent)->getBoundingSphere();
	}
	// TODO: add bounding sphere for all entities
	//Wolf::Debug::sendWarning("Getting bounding sphere of entity which doesn't contain a model component");
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

void Entity::setRotation(const glm::vec3& newRotation) const
{
	if (m_modelComponent)
	{
		(*m_modelComponent)->setRotation(newRotation);
		return;
	}

	Wolf::Debug::sendWarning("Trying to set rotation of entity which doesn't contain a model component");
}
