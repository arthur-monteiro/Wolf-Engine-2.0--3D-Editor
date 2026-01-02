#include "ComponentInstancier.h"

#include "Entity.h"

ComponentInstancier::ComponentInstancier(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
	std::function<void(ComponentInterface*)> requestReloadCallback, std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback, 
	const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const Wolf::ResourceNonOwner<AssetManager>& assetManager,const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager,
	const Wolf::ResourceNonOwner<EntityContainer>& entityContainer)
	: m_materialsGPUManager(materialsGPUManager),
      m_renderingPipeline(renderingPipeline),
      m_requestReloadCallback(std::move(requestReloadCallback)),
	  m_getEntityFromLoadingPathCallback(std::move(getEntityFromLoadingPathCallback)),
	  m_editorConfiguration(editorConfiguration),
	  m_assetManager(assetManager),
      m_physicsManager(physicsManager),
	  m_entityContainer(entityContainer)
{
}

ComponentInterface* ComponentInstancier::instanciateComponent(const std::string& componentId) const
{
	for (const ComponentInfo& componentInfo : m_componentsInfo)
	{
		if (componentInfo.id == componentId)
			return componentInfo.instancingFunction();
	}

	return nullptr;
}

std::string ComponentInstancier::getAllComponentTypes(const Wolf::ResourceNonOwner<Entity>& selectedEntity) const
{
	std::string r = R"({ "components": [)";
	for (const ComponentInfo& componentInfo : m_componentsInfo)
	{
		// Model interface check
		if ((componentInfo.id == StaticModel::ID) && selectedEntity->hasModelComponent())
			continue;

		// Same component check
		if (selectedEntity->hasComponent(componentInfo.id))
			continue;

		r += R"({ "name":")" + componentInfo.name + R"(", "value":")" + componentInfo.id + R"(" })";
		r += ", ";
	}
	r = r.substr(0, r.size() - 2);
	r += R"( ] })";

	return r;
}
