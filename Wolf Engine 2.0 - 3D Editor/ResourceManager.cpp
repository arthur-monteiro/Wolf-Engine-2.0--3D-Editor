#include "ResourceManager.h"

#include <fstream>

#include <Timer.h>

#include "EditorConfiguration.h"
#include "RenderMeshList.h"

ResourceManager::ResourceManager(const std::function<void(const std::string&, const std::string&)>& addResourceToUICallback, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, 
                                 const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
	: m_addResourceToUICallback(addResourceToUICallback), m_materialsGPUManager(materialsGPUManager), m_thumbnailsGenerationPass(renderingPipeline->getThumbnailsGenerationPass())
{
}

void ResourceManager::updateBeforeFrame()
{
	for (Wolf::ResourceUniqueOwner<Mesh>& mesh : m_meshes)
	{
		mesh->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}
}

void ResourceManager::clear()
{
	m_meshes.clear();
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
	if (const std::ifstream iconFile(iconFullPath.c_str()); iconFile.good())
	{
		iconFullPath = iconFullPath.substr(3, iconFullPath.size()); // remove "UI/"
	}
	else
	{
		iconFullPath = "media/resourceIcon/no_icon.png";
	}

	m_meshes.emplace_back(new Mesh(loadingPath, iconFullPath == "media/resourceIcon/no_icon.png"));

	m_addResourceToUICallback(m_meshes.back()->computeName(), iconFullPath);

	return static_cast<uint32_t>(m_meshes.size()) - 1 + MESH_RESOURCE_IDX_OFFSET;
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
	return "UI/media/resourceIcon/" + computeModelFullIdentifier(loadingPath) + ".png";
}

ResourceManager::ResourceInterface::ResourceInterface(std::string loadingPath) : m_loadingPath(std::move(loadingPath))
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

ResourceManager::Mesh::Mesh(const std::string& loadingPath, bool needThumbnailsGeneration) : ResourceInterface(loadingPath)
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

	if (m_thumbnailGenerationRequested && m_modelData.animationData.get() == nullptr /* animated meshes thumbnail generation is not supported yet */)
	{
		thumbnailsGenerationPass->addRequestBeforeFrame({ &m_modelData, computeIconPath(m_loadingPath) }); // TODO: once generated, refresh the icon
		m_thumbnailGenerationRequested = false;
	}
}
