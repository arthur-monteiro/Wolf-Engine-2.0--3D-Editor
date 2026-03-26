#include "ExternalSceneComponent.h"

#include <glm/gtx/matrix_decompose.hpp>

#include <ProfilerCommon.h>

#include "CommonLayouts.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"
#include "ExternalSceneLoader.h"
#include "StaticModel.h"

ExternalSceneComponent::ExternalSceneComponent(const Wolf::ResourceNonOwner<AssetManager>& assetManager, const std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)>& getEntityCallback,
                                               const std::function<Entity*(ComponentInterface*, const std::string&)>& createEntityCallback, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
                                               const std::function<void(ComponentInterface*)>& requestReloadCallback)
: m_assetManager(assetManager), m_getEntityCallback(getEntityCallback), m_createEntityCallback(createEntityCallback), m_materialsGPUManager(materialsGPUManager), m_requestReloadCallback(requestReloadCallback)
{
}

void ExternalSceneComponent::loadParams(Wolf::JSONReader& jsonReader)
{
	EditorModelInterface::loadParams(jsonReader, ID);
    ::loadParams(jsonReader, ID, m_editorParams);
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

        	std::filesystem::path scenePath(g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPathParam));
        	std::filesystem::path sceneFilename = currentEntityPath.filename();

        	std::string newEntitiesFolder = EditorConfiguration::sanitizeFilePath(currentEntityDirectory.string() + "/" + sceneFilename.string() + "_children");

        	if (!std::filesystem::is_directory(newEntitiesFolder) || !std::filesystem::exists(newEntitiesFolder))
        	{
        		std::filesystem::create_directory(newEntitiesFolder);
        	}

        	for (uint32_t i = 0; i < instances.size(); i++)
        	{
        		const ExternalSceneLoader::InstanceData& instance = instances[i];

        		AssetId modelAssetId = modelAssetsId[instance.m_meshIdx];

        		std::string modelName = m_assetManager->computeModelName(modelAssetId);

        		std::string newEntityPath = newEntitiesFolder + "/" + modelName + "_" + std::to_string(i) + ".json";
        		std::string newEntityLocalPath = g_editorConfiguration->computeLocalPathFromFullPath(newEntityPath);

        		if (Wolf::NullableResourceNonOwner<Entity> entity = m_getEntityCallback(newEntityLocalPath); static_cast<bool>(entity))
        		{
        			Wolf::NullableResourceNonOwner<StaticModel> staticModel = entity->getComponent<StaticModel>();
        			if (!staticModel)
        				Wolf::Debug::sendCriticalError("Entity should have static model component");
        			staticModel->setInfoFromParent(modelAssetId);

        			continue;
        		}

        		Entity* newEntity = m_createEntityCallback(this, newEntityLocalPath);
        		newEntity->setName(modelName + "_" + std::to_string(i));

        		StaticModel* staticModelComponent = new StaticModel(m_materialsGPUManager, m_assetManager, m_requestReloadCallback, m_getEntityCallback);
        		newEntity->addComponent(staticModelComponent);

        		staticModelComponent->setInfoFromParent(modelAssetId);

        		glm::vec3 scale;
        		glm::quat rotation;
        		glm::vec3 translation;
        		glm::vec3 skew;
        		glm::vec4 perspective;
        		if (glm::decompose(instance.m_transform, scale, rotation, translation, skew, perspective))
        		{
        			staticModelComponent->setPosition(translation);
        			staticModelComponent->setScale(scale);
        			staticModelComponent->setRotation(glm::conjugate(rotation));
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
		if (!m_assetManager->isModelLoaded(modelAssetId))
		{
			hasAMeshNotLoaded = true;
		}
	}

	if (hasAMeshNotLoaded)
		return false;

	return true;
}

void ExternalSceneComponent::requestSceneLoading()
{
    std::string loadingPathStr = std::string(m_loadingPathParam);
    if (!loadingPathStr.empty())
    {
        if (std::string sanitizedLoadingPath = EditorConfiguration::sanitizeFilePath(loadingPathStr); sanitizedLoadingPath != loadingPathStr)
        {
            m_loadingPathParam = sanitizedLoadingPath;
            return;
        }

        m_sceneAssetId = m_assetManager->addExternalScene(loadingPathStr);
        m_isWaitingForSceneLoading = true;
        notifySubscribers();
    }
}
