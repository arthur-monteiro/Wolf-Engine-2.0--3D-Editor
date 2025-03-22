#include "ResourceManager.h"

#include <fstream>

#include <Timer.h>

#include "EditorConfiguration.h"
#include "Entity.h"
#include "MeshResourceEditor.h"
#include "RenderMeshList.h"

ResourceManager::ResourceManager(const std::function<void(const std::string&, const std::string&, ResourceId)>& addResourceToUICallback, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback, 
	const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback,
	const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
	: m_addResourceToUICallback(addResourceToUICallback), m_updateResourceInUICallback(updateResourceInUICallback), m_requestReloadCallback(requestReloadCallback), m_editorConfiguration(editorConfiguration),
      m_materialsGPUManager(materialsGPUManager), m_thumbnailsGenerationPass(renderingPipeline->getThumbnailsGenerationPass())
{
}

void ResourceManager::updateBeforeFrame()
{
	for (Wolf::ResourceUniqueOwner<Mesh>& mesh : m_meshes)
	{
		mesh->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}
}

void ResourceManager::save()
{
	if (!findMeshResourceEditorInResourceEditionToSave(m_currentResourceInEdition))
	{
		addCurrentResourceEditionToSave();
	}

	for(uint32_t i = 0; i < m_resourceEditedParamsToSaveCount; ++i)
	{
		ResourceEditedParamsToSave& resourceEditedParamsToSave = m_resourceEditedParamsToSave[i];

		std::string fileContent;
		resourceEditedParamsToSave.meshResourceEditor->computeOutputJSON(fileContent);

		std::string outputFilename = m_meshes[resourceEditedParamsToSave.resourceId - MESH_RESOURCE_IDX_OFFSET]->getLoadingPath() + ".physics.json";

		std::ofstream outputFile;
		outputFile.open(m_editorConfiguration->computeFullPathFromLocalPath(outputFilename));
		outputFile << fileContent;
		outputFile.close();
	}
}

void ResourceManager::clear()
{
	m_meshes.clear();
}

Wolf::ResourceNonOwner<Entity> ResourceManager::computeResourceEditor(ResourceId resourceId)
{
	if (m_currentResourceInEdition == resourceId)
		return m_transientEditionEntity.createNonOwnerResource();

	MeshResourceEditor* meshResourceEditor = findMeshResourceEditorInResourceEditionToSave(resourceId);
	if (!meshResourceEditor)
	{
		if (m_resourceEditedParamsToSaveCount == MAX_EDITED_RESOURCES - 1)
		{
			Wolf::Debug::sendWarning("Maximum edited resources reached");
			return m_transientEditionEntity.createNonOwnerResource();
		}

		addCurrentResourceEditionToSave();
	}

	m_currentResourceInEdition = resourceId;

	m_transientEditionEntity.reset(new Entity("", [](Entity*) {}));
	m_transientEditionEntity->setIncludeEntityParams(false);
	if (!meshResourceEditor)
	{
		Wolf::ModelData* modelData = getModelData(resourceId);
		meshResourceEditor = new MeshResourceEditor(m_requestReloadCallback, modelData->isMeshCentered);

		for (Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& shape : modelData->physicsShapes)
		{
			meshResourceEditor->addShape(shape);
		}

		meshResourceEditor->subscribe(this, [this, resourceId, meshResourceEditor]() { onResourceEditionChanged(resourceId, meshResourceEditor); });
	}
	m_transientEditionEntity->addComponent(meshResourceEditor);

	return m_transientEditionEntity.createNonOwnerResource();
}

ResourceManager::ResourceId ResourceManager::addModel(const std::string& loadingPath)
{
	for (uint32_t i = 0; i < m_meshes.size(); ++i)
	{
		if (m_meshes[i]->getLoadingPath() == loadingPath)
		{
			return i + MESH_RESOURCE_IDX_OFFSET;
		}
	}

	std::string iconFullPath = computeIconPath(loadingPath);
	bool iconFileExists = false;
	if (const std::ifstream iconFile(iconFullPath.c_str()); iconFile.good())
	{
		iconFullPath = iconFullPath.substr(3, iconFullPath.size()); // remove "UI/"
		iconFileExists = true;
	}
	else
	{
		if (loadingPath.substr(loadingPath.find_last_of('.') + 1) == "dae")
		{
			iconFullPath = "media/resourceIcon/no_icon.gif";
		}
		else
		{
			iconFullPath = "media/resourceIcon/no_icon.png";
		}
	}

	ResourceId resourceId = static_cast<uint32_t>(m_meshes.size()) + MESH_RESOURCE_IDX_OFFSET;

	m_meshes.emplace_back(new Mesh(loadingPath, !iconFileExists, resourceId, m_updateResourceInUICallback));
	m_addResourceToUICallback(m_meshes.back()->computeName(), iconFullPath, resourceId);

	return resourceId;
}

bool ResourceManager::isModelLoaded(ResourceId modelResourceId) const
{
	return modelResourceId != NO_RESOURCE && m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->isLoaded();
}

Wolf::ModelData* ResourceManager::getModelData(ResourceId modelResourceId) const
{
	return m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->getModelData();
}

uint32_t ResourceManager::getFirstMaterialIdx(ResourceId modelResourceId) const
{
	return m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->getFirstMaterialIdx();
}

uint32_t ResourceManager::getFirstTextureSetIdx(ResourceId modelResourceId) const
{
	return m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->getFirstTextureSetIdx();
}

void ResourceManager::subscribeToResource(ResourceId resourceId, const void* instance, const std::function<void()>& callback) const
{
	m_meshes[resourceId - MESH_RESOURCE_IDX_OFFSET]->subscribe(instance, callback);
}

std::string ResourceManager::computeModelFullIdentifier(const std::string& loadingPath)
{
	std::string modelFullIdentifier;
	for (char character : loadingPath)
	{
		if (character == ' ' || character == '/' || character == '\\')
			modelFullIdentifier += '_';
		else
			modelFullIdentifier += character;
	}

	return modelFullIdentifier;
}

std::string ResourceManager::computeIconPath(const std::string& loadingPath)
{
	return "UI/media/resourceIcon/" + computeModelFullIdentifier(loadingPath) + ((loadingPath.substr(loadingPath.find_last_of('.') + 1) == "dae") ? ".gif" : ".png");
}

MeshResourceEditor* ResourceManager::findMeshResourceEditorInResourceEditionToSave(ResourceId resourceId)
{
	MeshResourceEditor* meshResourceEditor = nullptr;
	for (uint32_t i = 0; i < m_resourceEditedParamsToSaveCount; ++i)
	{
		ResourceEditedParamsToSave& resourceEditedParamsToSave = m_resourceEditedParamsToSave[i];

		if (resourceEditedParamsToSave.resourceId == resourceId) \
		{
			meshResourceEditor = resourceEditedParamsToSave.meshResourceEditor.release();
			break;
		}
	}

	return meshResourceEditor;
}

void ResourceManager::addCurrentResourceEditionToSave()
{
	if (m_currentResourceInEdition != NO_RESOURCE)
	{
		m_resourceEditedParamsToSaveCount++;
		ResourceEditedParamsToSave& resourceEditedParamsToSave = m_resourceEditedParamsToSave[m_resourceEditedParamsToSaveCount - 1];
		resourceEditedParamsToSave.resourceId = m_currentResourceInEdition;
		resourceEditedParamsToSave.meshResourceEditor.reset(m_transientEditionEntity->releaseComponent<MeshResourceEditor>());

		resourceEditedParamsToSave.meshResourceEditor->unregisterEntity();
	}
}

void ResourceManager::onResourceEditionChanged(ResourceId resourceId, MeshResourceEditor* meshResourceEditor)
{
	Wolf::ModelData* modelData = m_meshes[resourceId]->getModelData();
	modelData->physicsShapes.clear();

	for (uint32_t i = 0; i < meshResourceEditor->getMeshCount(); ++i)
	{
		Wolf::Physics::PhysicsShapeType shapeType = meshResourceEditor->getShapeType(i);

		if (shapeType == Wolf::Physics::PhysicsShapeType::Rectangle)
		{
			glm::vec3 p0;
			glm::vec3 s1;
			glm::vec3 s2;
			meshResourceEditor->computeInfoForRectangle(i, p0, s1, s2);

			modelData->physicsShapes.emplace_back(new Wolf::Physics::Rectangle(meshResourceEditor->getShapeName(i), p0, s1, s2));
		}
	}

	m_meshes[resourceId]->onChanged();
}

ResourceManager::ResourceInterface::ResourceInterface(std::string loadingPath, ResourceId resourceId, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback) 
	: m_loadingPath(std::move(loadingPath)), m_resourceId(resourceId), m_updateResourceInUICallback(updateResourceInUICallback)
{
}

std::string ResourceManager::ResourceInterface::computeName()
{
    std::string::size_type pos = m_loadingPath.find_last_of('\\');
    if (pos != std::string::npos)
    {
        return m_loadingPath.substr(pos);
    }
    else
    {
        return m_loadingPath;
    }
}

void ResourceManager::ResourceInterface::onChanged()
{
	notifySubscribers();
}

ResourceManager::Mesh::Mesh(const std::string& loadingPath, bool needThumbnailsGeneration, ResourceId resourceId, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback)
	: ResourceInterface(loadingPath, resourceId, updateResourceInUICallback)
{
	m_modelLoadingRequested = true;
	m_thumbnailGenerationRequested = needThumbnailsGeneration;
}

void ResourceManager::Mesh::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	if (m_modelLoadingRequested)
	{
		loadModel(materialsGPUManager, thumbnailsGenerationPass);
		m_modelLoadingRequested = false;
	}
}

bool ResourceManager::Mesh::isLoaded() const
{
	return static_cast<bool>(m_modelData.mesh);
}

void ResourceManager::Mesh::loadModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath);

	Wolf::Timer timer(std::string(m_loadingPath) + " loading");

	Wolf::ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = fullFilePath;
	modelLoadingInfo.mtlFolder = fullFilePath.substr(0, fullFilePath.find_last_of('\\'));
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.textureSetLayout = Wolf::TextureSetLoader::InputTextureSetLayout::EACH_TEXTURE_A_FILE;
	Wolf::ModelLoader::loadObject(m_modelData, modelLoadingInfo);

	m_firstTextureSetIdx = m_modelData.textureSets.empty() ? 0 : materialsGPUManager->getCurrentTextureSetCount();
	m_firstMaterialIdx = m_modelData.textureSets.empty() ? 0 : materialsGPUManager->getCurrentMaterialCount();
	for (const Wolf::MaterialsGPUManager::TextureSetInfo& textureSetInfo : m_modelData.textureSets)
	{
		materialsGPUManager->addNewTextureSet(textureSetInfo);

		Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
		materialInfo.textureSetInfos[0].textureSetIdx = materialsGPUManager->getTextureSetsCacheInfo().back().textureSetIdx;
		materialsGPUManager->addNewMaterial(materialInfo);
	}

	if (m_thumbnailGenerationRequested)
	{
		thumbnailsGenerationPass->addRequestBeforeFrame({ &m_modelData, computeIconPath(m_loadingPath), 
			[this]() { m_updateResourceInUICallback(computeName(), computeIconPath(m_loadingPath).substr(3, computeIconPath(m_loadingPath).size()), m_resourceId); } });
		m_thumbnailGenerationRequested = false;
	}
}
