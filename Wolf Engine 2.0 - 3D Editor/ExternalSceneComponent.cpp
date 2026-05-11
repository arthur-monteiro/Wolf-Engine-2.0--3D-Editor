#include "ExternalSceneComponent.h"

#include <glm/gtx/matrix_decompose.hpp>

#include <ProfilerCommon.h>

#include "AnimatedMesh.h"
#include "CommonLayouts.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"
#include "ExternalSceneLoader.h"
#include "StaticMesh.h"

ExternalSceneComponent::ExternalSceneComponent(const Wolf::ResourceNonOwner<AssetManager>& assetManager, const std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)>& getEntityCallback,
                                               const std::function<Entity*(ComponentInterface*, const std::string&)>& createEntityCallback, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
                                               const std::function<void(ComponentInterface*)>& requestReloadCallback, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
: m_assetManager(assetManager), m_getEntityCallback(getEntityCallback), m_createEntityCallback(createEntityCallback), m_materialsGPUManager(materialsGPUManager),
  m_requestReloadCallback(requestReloadCallback), m_renderingPipeline(renderingPipeline)
{
}

void ExternalSceneComponent::loadParams(Wolf::JSONReader& jsonReader)
{
	EditorModelInterface::loadParams(jsonReader, ID);
    ::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_editorParams);
}

void ExternalSceneComponent::activateParams()
{
    for (EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->activate();
    }
}

void ExternalSceneComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
    for (const EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->addToJSON(outJSON, tabCount, false);
    }
}

void ExternalSceneComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
    if (m_isWaitingForSceneLoading)
    {
        if (m_assetManager->isSceneLoaded(m_sceneAssetId))
        {
        	const std::vector<ExternalSceneLoader::InstanceData>& instances = m_assetManager->getSceneInstances(m_sceneAssetId);
        	const std::vector<AssetId> modelAssetsId = m_assetManager->getSceneModelAssetIds(m_sceneAssetId);

        	std::filesystem::path currentEntityPath(g_editorConfiguration->computeFullPathFromLocalPath(m_entity->getLoadingPath()));
        	std::filesystem::path currentEntityDirectory = currentEntityPath.parent_path();

        	std::filesystem::path scenePath(g_editorConfiguration->computeFullPathFromLocalPath(m_assetParam));
        	std::filesystem::path sceneFilename = currentEntityPath.filename();

        	std::string newEntitiesFolder = EditorConfiguration::sanitizeFilePath(currentEntityDirectory.string() + "/" + sceneFilename.string() + "_children");

        	for (uint32_t i = 0; i < instances.size(); i++)
        	{
        		const ExternalSceneLoader::InstanceData& instance = instances[i];

        		AssetId modelAssetId = modelAssetsId[instance.m_meshIdx];

        		std::string modelName = m_assetManager->computeModelName(modelAssetId);
        		bool isAnimated = m_assetManager->isMeshAnimated(modelAssetId);

        		std::string newEntityPath = newEntitiesFolder + "/" + modelName + "_" + std::to_string(i) + ".json";
        		std::string newEntityLocalPath = g_editorConfiguration->computeLocalPathFromFullPath(newEntityPath);

        		if (Wolf::NullableResourceNonOwner<Entity> entity = m_getEntityCallback(newEntityLocalPath); static_cast<bool>(entity))
        		{
        			Wolf::Debug::sendCriticalError("Entity should not already exist");
        		}

        		Entity* newEntity = m_createEntityCallback(this, newEntityLocalPath);
        		newEntity->setName(modelName + "_" + std::to_string(i));
        		newEntity->setTransient();

        		StaticMesh* staticModelComponent = nullptr;
        		AnimatedMesh* animatedModelComponent = nullptr;

        		if (isAnimated)
        		{
        			animatedModelComponent = new AnimatedMesh(m_assetManager, m_getEntityCallback, m_renderingPipeline, m_requestReloadCallback);
        			newEntity->addComponent(animatedModelComponent);
        			animatedModelComponent->setInfoFromParent(modelAssetId);
        		}
        		else
        		{
        			staticModelComponent = new StaticMesh(m_assetManager);
        			newEntity->addComponent(staticModelComponent);
        			staticModelComponent->setInfoFromParent(modelAssetId);
        		}

        		glm::vec3 scale;
        		glm::quat rotation;
        		glm::vec3 translation;
        		glm::vec3 skew;
        		glm::vec4 perspective;
        		if (glm::decompose(instance.m_transform, scale, rotation, translation, skew, perspective))
        		{
        			if (staticModelComponent)
        			{
        				staticModelComponent->setPosition(translation);
        				staticModelComponent->setScale(scale);
        				staticModelComponent->setRotation(glm::conjugate(rotation));
        			}
        			else
        			{
        				animatedModelComponent->setPosition(translation);
        				animatedModelComponent->setScale(scale);
        				animatedModelComponent->setRotation(glm::conjugate(rotation));
        			}
        		}
        		else
        		{
        			Wolf::Debug::sendCriticalError("Can't decompose matrix");
        		}
        	}

            m_isWaitingForSceneLoading = false;
        }
    }
}

Wolf::AABB ExternalSceneComponent::getAABB() const
{
    if (m_assetManager->isSceneLoaded(m_sceneAssetId))
        return m_assetManager->getSceneAABB(m_sceneAssetId) * m_transform;

    return Wolf::AABB();
}

Wolf::BoundingSphere ExternalSceneComponent::getBoundingSphere() const
{
    return Wolf::BoundingSphere(getAABB());
}

bool ExternalSceneComponent::areAllMeshLoaded()
{
	if (!m_assetManager->isSceneLoaded(m_sceneAssetId))
		return false;

	const std::vector<AssetId> modelAssetsId = m_assetManager->getSceneModelAssetIds(m_sceneAssetId);

	bool hasAMeshNotLoaded = false;
	for (AssetId modelAssetId : modelAssetsId)
	{
		if (!m_assetManager->isMeshLoaded(modelAssetId))
		{
			hasAMeshNotLoaded = true;
		}
	}

	if (hasAMeshNotLoaded)
		return false;

	return true;
}

void ExternalSceneComponent::onAssetChanged()
{
	if (static_cast<std::string>(m_assetParam) == "")
		return;

	AssetId assetId = m_assetManager->getAssetIdForPath(m_assetParam);
	if (!m_assetManager->isExternalScene(assetId))
	{
		Wolf::Debug::sendWarning("Asset is not an external scene");
		m_assetParam = "";
	}

	m_sceneAssetId = assetId;
	m_isWaitingForSceneLoading = true;
	notifySubscribers();
}
