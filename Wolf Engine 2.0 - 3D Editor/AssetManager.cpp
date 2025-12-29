#include "AssetManager.h"

#include <glm/gtc/packing.hpp>
#include <fstream>
#include <stb_image_write.h>

#include <ImageFileLoader.h>
#include <MipMapGenerator.h>

#include <ProfilerCommon.h>
#include <Timer.h>

#include "EditorConfiguration.h"
#include "Entity.h"
#include "MeshAssetEditor.h"
#include "RenderMeshList.h"

AssetManager::AssetManager(const std::function<void(const std::string&, const std::string&, AssetId)>& addAssetToUICallback, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback, 
                                 const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback,
                                 const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<void(const std::string& loadingPath)>& isolateMeshCallback,
                                 const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback)
	: m_addResourceToUICallback(addAssetToUICallback), m_updateResourceInUICallback(updateResourceInUICallback), m_requestReloadCallback(requestReloadCallback), m_editorConfiguration(editorConfiguration),
      m_materialsGPUManager(materialsGPUManager), m_thumbnailsGenerationPass(renderingPipeline->getThumbnailsGenerationPass()), m_isolateMeshCallback(isolateMeshCallback), m_removeIsolationAndGetViewMatrixCallback(removeIsolationAndGetViewMatrixCallback),
	  m_renderingPipeline(renderingPipeline)
{
}

void AssetManager::updateBeforeFrame()
{
	PROFILE_FUNCTION

	for (Wolf::ResourceUniqueOwner<Mesh>& mesh : m_meshes)
	{
		mesh->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	for (Wolf::ResourceUniqueOwner<Image>& image : m_images)
	{
		image->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	if (m_currentAssetNeedRebuildFlags != 0)
	{
		if (m_currentAssetNeedRebuildFlags & static_cast<uint32_t>(MeshAssetEditor::ResourceEditorNotificationFlagBits::MESH))
		{
			m_meshes[m_currentAssetInEdition]->forceReload(m_materialsGPUManager, m_thumbnailsGenerationPass);
		}
		if (m_currentAssetNeedRebuildFlags & static_cast<uint32_t>(MeshAssetEditor::ResourceEditorNotificationFlagBits::PHYSICS))
		{
			Wolf::ModelData* modelData = m_meshes[m_currentAssetInEdition]->getModelData();
			modelData->m_physicsShapes.clear();

			for (uint32_t i = 0; i < m_meshAssetEditor->getPhysicsMeshCount(); ++i)
			{
				Wolf::Physics::PhysicsShapeType shapeType = m_meshAssetEditor->getShapeType(i);

				if (shapeType == Wolf::Physics::PhysicsShapeType::Rectangle)
				{
					glm::vec3 p0;
					glm::vec3 s1;
					glm::vec3 s2;
					m_meshAssetEditor->computeInfoForRectangle(i, p0, s1, s2);

					modelData->m_physicsShapes.emplace_back(new Wolf::Physics::Rectangle(m_meshAssetEditor->getShapeName(i), p0, s1, s2));
				}
				else if (shapeType == Wolf::Physics::PhysicsShapeType::Box)
				{
					glm::vec3 aabbMin;
					glm::vec3 aabbMax;
					glm::vec3 rotation;
					m_meshAssetEditor->computeInfoForBox(i, aabbMin, aabbMax, rotation);

					modelData->m_physicsShapes.emplace_back(new Wolf::Physics::Box(m_meshAssetEditor->getShapeName(i), aabbMin, aabbMax, rotation));
				}
			}
		}
		m_meshes[m_currentAssetInEdition]->onChanged();

		m_currentAssetNeedRebuildFlags = 0;
	}
}

void AssetManager::save()
{
	if (!findMeshResourceEditorInResourceEditionToSave(m_currentAssetInEdition))
	{
		addCurrentResourceEditionToSave();
	}

	for(uint32_t i = 0; i < m_assetEditedParamsToSaveCount; ++i)
	{
		ResourceEditedParamsToSave& resourceEditedParamsToSave = m_assetEditedParamsToSave[i];

		// Physics
		if (resourceEditedParamsToSave.meshAssetEditor->getPhysicsMeshCount() > 0)
		{
			std::string physicsFileContent;
			resourceEditedParamsToSave.meshAssetEditor->computePhysicsOutputJSON(physicsFileContent);

			std::string outputFilename = m_meshes[resourceEditedParamsToSave.assetId - MESH_ASSET_IDX_OFFSET]->getLoadingPath() + ".physics.json";

			std::ofstream outputFile;
			outputFile.open(m_editorConfiguration->computeFullPathFromLocalPath(outputFilename));
			outputFile << physicsFileContent;
			outputFile.close();
		}

		// Info
		{
			std::string infoFileContent;
			resourceEditedParamsToSave.meshAssetEditor->computeInfoOutputJSON(infoFileContent);

			std::string outputFilename = m_meshes[resourceEditedParamsToSave.assetId - MESH_ASSET_IDX_OFFSET]->getLoadingPath() + ".info.json";

			std::ofstream outputFile;
			outputFile.open(m_editorConfiguration->computeFullPathFromLocalPath(outputFilename));
			outputFile << infoFileContent;
			outputFile.close();
		}

		// TODO: computed vertex data
	}
}

void AssetManager::clear()
{
	m_meshes.clear();
	m_images.clear();
}

Wolf::ResourceNonOwner<Entity> AssetManager::computeResourceEditor(AssetId assetId)
{
	if (m_currentAssetInEdition == assetId)
		return m_transientEditionEntity.createNonOwnerResource();

	m_transientEditionEntity.reset(new Entity("", [](Entity*) {}, [](Entity*) {}));
	m_transientEditionEntity->setIncludeEntityParams(false);

	if (isMesh(assetId))
	{
		MeshAssetEditor* meshResourceEditor = findMeshResourceEditorInResourceEditionToSave(assetId);
		if (!meshResourceEditor)
		{
			if (m_assetEditedParamsToSaveCount == MAX_EDITED_ASSET - 1)
			{
				Wolf::Debug::sendWarning("Maximum edited resources reached");
				return m_transientEditionEntity.createNonOwnerResource();
			}

			addCurrentResourceEditionToSave();
		}

		if (!meshResourceEditor)
		{
			Wolf::ModelData* modelData = getModelData(assetId);
			uint32_t firstMaterialIdx = getFirstMaterialIdx(assetId);
			Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> bottomLevelAccelerationStructure = getBLAS(assetId, 0, 0);
			meshResourceEditor = new MeshAssetEditor(m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getLoadingPath(), m_requestReloadCallback, modelData, firstMaterialIdx, bottomLevelAccelerationStructure, m_isolateMeshCallback,
				m_removeIsolationAndGetViewMatrixCallback, [this, assetId](const glm::mat4& viewMatrix) { requestThumbnailReload(assetId, viewMatrix); }, m_renderingPipeline);

			for (Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& shape : modelData->m_physicsShapes)
			{
				meshResourceEditor->addShape(shape);
			}

			meshResourceEditor->subscribe(this, [this](Notifier::Flags flags) { onResourceEditionChanged(flags); });
		}
		m_transientEditionEntity->addComponent(meshResourceEditor);
		m_meshAssetEditor = m_transientEditionEntity->getComponent<MeshAssetEditor>();
	}
	else if (isImage(assetId))
	{
		Wolf::Debug::sendError("Image editor is not implemented yet");
	}
	else
	{
		Wolf::Debug::sendError("Resource type is not supported");
	}

	m_currentAssetInEdition = assetId;

	return m_transientEditionEntity.createNonOwnerResource();
}

bool AssetManager::isMesh(AssetId assetId)
{
	return assetId == NO_ASSET || (assetId >= MESH_ASSET_IDX_OFFSET && assetId < MESH_ASSET_IDX_OFFSET + MAX_ASSET_RESOURCE_COUNT);
}

bool AssetManager::isImage(AssetId assetId)
{
	return assetId == NO_ASSET || (assetId >= IMAGE_ASSET_IDX_OFFSET && assetId < IMAGE_ASSET_IDX_OFFSET + MAX_IMAGE_ASSET_COUNT);
}

AssetManager::AssetId AssetManager::addModel(const std::string& loadingPath)
{
	if (m_meshes.size() >= MAX_ASSET_RESOURCE_COUNT)
	{
		Wolf::Debug::sendError("Maximum mesh resources reached");
		return NO_ASSET;
	}

	for (uint32_t i = 0; i < m_meshes.size(); ++i)
	{
		if (m_meshes[i]->getLoadingPath() == loadingPath)
		{
			return i + MESH_ASSET_IDX_OFFSET;
		}
	}

	std::string iconFullPath = computeIconPath(loadingPath, 0);
	bool iconFileExists = formatIconPath(loadingPath, iconFullPath);
	uint32_t thumbnailsCount = 0;
	while (iconFileExists)
	{
		thumbnailsCount++;

		std::string nextIconFullPath = computeIconPath(loadingPath, thumbnailsCount);
		iconFileExists = formatIconPath(loadingPath, nextIconFullPath);
	}

	iconFileExists = thumbnailsCount > 0;
	if (thumbnailsCount > 1) // We need to replace the original with the latest one
	{
		std::string originalPath = computeIconPath(loadingPath, 0);
		std::string latestPath = computeIconPath(loadingPath, thumbnailsCount - 1);

		std::filesystem::remove(originalPath);
		std::filesystem::rename(latestPath, originalPath);
	}

	AssetId resourceId = static_cast<uint32_t>(m_meshes.size()) + MESH_ASSET_IDX_OFFSET;

	Mesh* newMesh = new Mesh(loadingPath, !iconFileExists, resourceId, m_updateResourceInUICallback);
	m_meshes.emplace_back(newMesh);
	m_addResourceToUICallback(m_meshes.back()->computeName(), iconFullPath, resourceId);

	std::string infoFilePath = g_editorConfiguration->computeFullPathFromLocalPath(loadingPath + ".info.json");
	bool infoFileExists = false;
	{
		std::ifstream infoFile(infoFilePath);
		if (infoFile.good())
		{
			infoFileExists = true;
		}
	}

	if (infoFileExists)
	{
		Wolf::JSONReader::FileReadInfo fileReadInfo(infoFilePath);
		Wolf::JSONReader infoJSON(fileReadInfo);

		Wolf::JSONReader::JSONObjectInterface* thumbnailGenerationInfo = infoJSON.getRoot()->getPropertyObject("thumbnailGenerationInfo");
		glm::mat4 viewMatrix;
		for (glm::length_t i = 0; i < 4; ++i)
		{
			for (glm::length_t j = 0; j < 4; ++j)
			{
				viewMatrix[i][j] = thumbnailGenerationInfo->getPropertyFloat("viewMatrix" + std::to_string(i) + std::to_string(j));
			}
		}

		newMesh->setThumbnailGenerationViewMatrix(viewMatrix);
	}

	return resourceId;
}

bool AssetManager::isModelLoaded(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return modelResourceId != NO_ASSET && m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->isLoaded();
}

Wolf::ModelData* AssetManager::getModelData(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getModelData();
}

Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> AssetManager::getBLAS(AssetId modelResourceId, uint32_t lod, uint32_t lodType)
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getBLAS(lod, lodType);
}

uint32_t AssetManager::getFirstMaterialIdx(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getFirstMaterialIdx();
}

uint32_t AssetManager::getFirstTextureSetIdx(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getFirstTextureSetIdx();
}

void AssetManager::subscribeToResource(AssetId resourceId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const
{
	if (!isMesh(resourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	m_meshes[resourceId - MESH_ASSET_IDX_OFFSET]->subscribe(instance, callback);
}

AssetManager::AssetId AssetManager::addImage(const std::string& loadingPath, bool loadMips, Wolf::Format format, bool keepDataOnCPU)
{
	if (m_images.size() >= MAX_IMAGE_ASSET_COUNT)
	{
		Wolf::Debug::sendError("Maximum image resources reached");
		return NO_ASSET;
	}

	for (uint32_t i = 0; i < m_images.size(); ++i)
	{
		if (m_images[i]->getLoadingPath() == loadingPath)
		{
			return i + IMAGE_ASSET_IDX_OFFSET;
		}
	}

	AssetId resourceId = static_cast<uint32_t>(m_images.size()) + IMAGE_ASSET_IDX_OFFSET;

	std::string iconFullPath = computeIconPath(loadingPath, 0);
	bool iconFileExists = formatIconPath(loadingPath, iconFullPath);

	m_images.emplace_back(new Image(loadingPath, !iconFileExists, resourceId, m_updateResourceInUICallback, loadMips, format, keepDataOnCPU));
	m_addResourceToUICallback(m_images.back()->computeName(), iconFullPath, resourceId);

	return resourceId;
}

bool AssetManager::isImageLoaded(AssetId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}

	return m_images[imageResourceId - IMAGE_ASSET_IDX_OFFSET]->isLoaded();
}

Wolf::ResourceNonOwner<Wolf::Image> AssetManager::getImage(AssetId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}

	return m_images[imageResourceId - IMAGE_ASSET_IDX_OFFSET]->getImage();
}

const uint8_t* AssetManager::getImageData(AssetId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}

	return m_images[imageResourceId - IMAGE_ASSET_IDX_OFFSET]->getFirstMipData();
}

void AssetManager::deleteImageData(AssetId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}
	m_images[imageResourceId - IMAGE_ASSET_IDX_OFFSET]->deleteImageData();
}

std::string AssetManager::computeModelFullIdentifier(const std::string& loadingPath)
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

std::string AssetManager::computeIconPath(const std::string& loadingPath, uint32_t thumbnailsLockedCount)
{
	std::string resourceExtension = loadingPath.substr(loadingPath.find_last_of('.') + 1);
	std::string thumbnailExtension = resourceExtension == "dae" || resourceExtension == "cube" ? ".gif" : ".png";

	std::string iconPathNoExtension = "UI/media/resourceIcon/" + computeModelFullIdentifier(loadingPath);
	if (thumbnailsLockedCount > 0)
	{
		iconPathNoExtension += "_" + std::to_string(thumbnailsLockedCount);
	}
	std::string iconPath = iconPathNoExtension + thumbnailExtension;
	return iconPath;
}

bool AssetManager::formatIconPath(const std::string& inLoadingPath, std::string& outIconPath)
{
	bool iconFileExists = false;
	if (const std::ifstream iconFile(outIconPath.c_str()); iconFile.good())
	{
		outIconPath = outIconPath.substr(3, outIconPath.size()); // remove "UI/"
		iconFileExists = true;
	}
	else
	{
		std::string extension = inLoadingPath.substr(inLoadingPath.find_last_of('.') + 1);
		if (extension == "dae" || extension == "cube")
		{
			outIconPath = "media/resourceIcon/no_icon.gif";
		}
		else
		{
			outIconPath = "media/resourceIcon/no_icon.png";
		}
	}

	return iconFileExists;
}

MeshAssetEditor* AssetManager::findMeshResourceEditorInResourceEditionToSave(AssetId resourceId)
{
	MeshAssetEditor* meshResourceEditor = nullptr;
	for (uint32_t i = 0; i < m_assetEditedParamsToSaveCount; ++i)
	{
		ResourceEditedParamsToSave& resourceEditedParamsToSave = m_assetEditedParamsToSave[i];

		if (resourceEditedParamsToSave.assetId == resourceId) \
		{
			meshResourceEditor = resourceEditedParamsToSave.meshAssetEditor.release();
			break;
		}
	}

	return meshResourceEditor;
}

void AssetManager::addCurrentResourceEditionToSave()
{
	if (isImage(m_currentAssetInEdition))
	{
		return; // nothing to save for images
	}

	if (m_currentAssetInEdition != NO_ASSET)
	{
		m_assetEditedParamsToSaveCount++;
		ResourceEditedParamsToSave& resourceEditedParamsToSave = m_assetEditedParamsToSave[m_assetEditedParamsToSaveCount - 1];
		resourceEditedParamsToSave.assetId = m_currentAssetInEdition;
		resourceEditedParamsToSave.meshAssetEditor.reset(m_transientEditionEntity->releaseComponent<MeshAssetEditor>());

		resourceEditedParamsToSave.meshAssetEditor->unregisterEntity();
	}
}

void AssetManager::onResourceEditionChanged(Notifier::Flags flags)
{
	m_currentAssetNeedRebuildFlags |= flags;
}

void AssetManager::requestThumbnailReload(AssetId resourceId, const glm::mat4& viewMatrix)
{
	if (isMesh(resourceId))
	{
		m_meshes[resourceId - MESH_ASSET_IDX_OFFSET]->setThumbnailGenerationViewMatrix(viewMatrix);
		m_meshes[resourceId - MESH_ASSET_IDX_OFFSET]->requestThumbnailReload();
	}
}

AssetManager::ResourceInterface::ResourceInterface(std::string loadingPath, AssetId resourceId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback) 
	: m_loadingPath(std::move(loadingPath)), m_resourceId(resourceId), m_updateAssetInUICallback(updateResourceInUICallback)
{
}

std::string AssetManager::ResourceInterface::computeName()
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

void AssetManager::ResourceInterface::onChanged()
{
	notifySubscribers();
}

AssetManager::Mesh::Mesh(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId resourceId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback)
	: ResourceInterface(loadingPath, resourceId, updateResourceInUICallback)
{
	m_modelLoadingRequested = true;
	m_thumbnailGenerationRequested = needThumbnailsGeneration;
}

void AssetManager::Mesh::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	if (m_modelLoadingRequested)
	{
		loadModel(materialsGPUManager);
		m_modelLoadingRequested = false;
	}
	if (m_thumbnailGenerationRequested)
	{
		generateThumbnail(thumbnailsGenerationPass);
		m_thumbnailGenerationRequested = false;
	}

	if (m_meshToKeepInMemory)
	{
		m_meshToKeepInMemory.reset(nullptr);
	}
}

void AssetManager::Mesh::forceReload(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
	const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	m_meshToKeepInMemory.reset(m_modelData.m_mesh.release());

	loadModel(materialsGPUManager);
	generateThumbnail(thumbnailsGenerationPass);
	m_modelLoadingRequested = false;
}

void AssetManager::Mesh::requestThumbnailReload()
{
	std::string iconPath = computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);
	if (std::filesystem::exists(iconPath))
	{
		std::error_code ec;
		if (!std::filesystem::remove(iconPath, ec))
		{
			// Thumbnail file exists but is OS locked, we need to create a new file with a new name
			m_thumbnailCountToMaintain++;
		}
	}

	m_thumbnailGenerationRequested = true;
}

bool AssetManager::Mesh::isLoaded() const
{
	return static_cast<bool>(m_modelData.m_mesh);
}

Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> AssetManager::Mesh::getBLAS(uint32_t lod, uint32_t lodType)
{
	if (lodType >= m_bottomLevelAccelerationStructures.size() || lod >= m_bottomLevelAccelerationStructures[lodType].size())
	{
		return Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure>();
	}
	return m_bottomLevelAccelerationStructures[lodType][lod].createNonOwnerResource();
}

void AssetManager::Mesh::loadModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager)
{
	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath);

	Wolf::Timer timer(std::string(m_loadingPath) + " loading");

	Wolf::ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = fullFilePath;
	modelLoadingInfo.mtlFolder = fullFilePath.substr(0, fullFilePath.find_last_of('\\'));
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.textureSetLayout = Wolf::TextureSetLoader::InputTextureSetLayout::EACH_TEXTURE_A_FILE;
	if (g_editorConfiguration->getEnableRayTracing())
	{
		VkBufferUsageFlags rayTracingFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		modelLoadingInfo.additionalVertexBufferUsages |= rayTracingFlags;
		modelLoadingInfo.additionalIndexBufferUsages |= rayTracingFlags;
	}
	modelLoadingInfo.generateDefaultLODCount = 8;
	modelLoadingInfo.generateSloppyLODCount= 16;
	Wolf::ModelLoader::loadObject(m_modelData, modelLoadingInfo);

	materialsGPUManager->lockMaterials();
	materialsGPUManager->lockTextureSets();

	m_firstTextureSetIdx = m_modelData.m_textureSets.empty() ? 0 : materialsGPUManager->getCurrentTextureSetCount();
	m_firstMaterialIdx = m_modelData.m_textureSets.empty() ? 0 : materialsGPUManager->getCurrentMaterialCount();
	for (const Wolf::MaterialsGPUManager::TextureSetInfo& textureSetInfo : m_modelData.m_textureSets)
	{
		materialsGPUManager->addNewTextureSet(textureSetInfo);

		Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
		materialInfo.textureSetInfos[0].textureSetIdx = materialsGPUManager->getTextureSetsCacheInfo().back().textureSetIdx;
		materialsGPUManager->addNewMaterial(materialInfo);
	}

	materialsGPUManager->unlockTextureSets();
	materialsGPUManager->unlockMaterials();

	if (g_editorConfiguration->getEnableRayTracing())
	{
		m_bottomLevelAccelerationStructures.resize(2);
		for (uint32_t lodType = 0; lodType < 2; lodType++)
		{
			uint32_t lodCount = lodType == 0 ? m_modelData.m_defaultSimplifiedIndexBuffers.size() : m_modelData.m_sloppySimplifiedIndexBuffers.size();

			m_bottomLevelAccelerationStructures[lodType].resize(lodCount + 1);
			for (uint32_t lod = 0; lod < m_bottomLevelAccelerationStructures[lodType].size(); lod++)
			{
				buildBLAS(lod, lodType);
			}
		}
	}

	Wolf::AABB entityAABB = m_modelData.m_mesh->getAABB();
	float entityHeight = entityAABB.getMax().y - entityAABB.getMin().y;
	glm::vec3 position = entityAABB.getCenter() + glm::vec3(-entityHeight, entityHeight, -entityHeight);
	glm::vec3 target = entityAABB.getCenter();
	m_thumbnailGenerationViewMatrix = glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void AssetManager::Mesh::generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	std::string iconPath = computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);

	thumbnailsGenerationPass->addRequestBeforeFrame({ &m_modelData, m_firstMaterialIdx, iconPath,
		[this, iconPath]() { m_updateAssetInUICallback(computeName(), iconPath.substr(3, iconPath.size()), m_resourceId); },
			m_thumbnailGenerationViewMatrix });
}

void AssetManager::Mesh::buildBLAS(uint32_t lod, uint32_t lodType)
{
	Wolf::GeometryInfo geometryInfo;
	geometryInfo.mesh.vertexBuffer = &*m_modelData.m_mesh->getVertexBuffer();
	geometryInfo.mesh.vertexCount = m_modelData.m_mesh->getVertexCount();
	geometryInfo.mesh.vertexSize = m_modelData.m_mesh->getVertexSize();
	geometryInfo.mesh.vertexFormat = Wolf::Format::R32G32B32_SFLOAT;
	if (lod == 0)
	{
		geometryInfo.mesh.indexBuffer = &*m_modelData.m_mesh->getIndexBuffer();
		geometryInfo.mesh.indexCount = m_modelData.m_mesh->getIndexCount();
	}
	else
	{
		Wolf::ResourceUniqueOwner<Wolf::Buffer>* overrideIndexBuffer = nullptr;
		if (lodType == 0)
		{
			overrideIndexBuffer = &m_modelData.m_defaultSimplifiedIndexBuffers[lod - 1];
		}
		else
		{
			overrideIndexBuffer = &m_modelData.m_sloppySimplifiedIndexBuffers[lod - 1];
		}

		geometryInfo.mesh.indexBuffer = &**overrideIndexBuffer;
		uint32_t indexCount = (*overrideIndexBuffer)->getSize() / sizeof(uint32_t);
		geometryInfo.mesh.indexCount = indexCount;
	}

	Wolf::BottomLevelAccelerationStructureCreateInfo createInfo{};
	createInfo.geometryInfos = { &geometryInfo, 1 };
	createInfo.buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

	m_bottomLevelAccelerationStructures[lodType][lod].reset(Wolf::BottomLevelAccelerationStructure::createBottomLevelAccelerationStructure(createInfo));
}

AssetManager::Image::Image(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId resourceId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback,
	bool loadMips, Wolf::Format format, bool keepDataOnCPU)
	: ResourceInterface(loadingPath, resourceId, updateResourceInUICallback), m_thumbnailGenerationRequested(needThumbnailsGeneration), m_loadMips(loadMips), m_format(format)
{
	m_imageLoadingRequested = true;
	m_dataOnCPUStatus = keepDataOnCPU ? DataOnCPUStatus::NOT_LOADED_YET : DataOnCPUStatus::NEVER_KEPT;
}

void AssetManager::Image::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	if (m_imageLoadingRequested)
	{
		loadImage();
		m_imageLoadingRequested = false;
	}
}

bool AssetManager::Image::isLoaded() const
{
	return static_cast<bool>(m_image);
}

const uint8_t* AssetManager::Image::getFirstMipData() const
{
	if (m_dataOnCPUStatus != DataOnCPUStatus::AVAILABLE)
	{
		Wolf::Debug::sendError("Data requested in not here");
		return nullptr;
	}

	return m_firstMipData.data();
}

void AssetManager::Image::deleteImageData()
{
	m_firstMipData.clear();
	m_dataOnCPUStatus = DataOnCPUStatus::DELETED;
}

void AssetManager::Image::loadImage()
{
	bool isHDR = false;
	if (m_format == Wolf::Format::R32G32B32A32_SFLOAT)
	{
		isHDR = true;
	}

	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath);
	Wolf::ImageFileLoader imageFileLoader(fullFilePath, isHDR);

	Wolf::CreateImageInfo createImageInfo;
	createImageInfo.extent = { imageFileLoader.getWidth(), imageFileLoader.getHeight(), imageFileLoader.getDepth() };
	createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	Wolf::Format imageFileLoaderFormat = imageFileLoader.getFormat();

	if (imageFileLoaderFormat == Wolf::Format::R8G8B8A8_UNORM && m_format == Wolf::Format::R8G8B8A8_SRGB)
	{
		imageFileLoaderFormat = Wolf::Format::R8G8B8A8_SRGB;
	}

	Wolf::ResourceUniqueOwner<Wolf::MipMapGenerator> mipMapGenerator;
	uint32_t mipLevelCount = 1;
	if (m_loadMips)
	{
		if (imageFileLoader.getDepth())
		{
			Wolf::Debug::sendError("Load mip with 3D images is not supported yet");
		}

		mipMapGenerator.reset(new Wolf::MipMapGenerator(imageFileLoader.getPixels(), { imageFileLoader.getWidth(), imageFileLoader.getHeight() }, imageFileLoaderFormat));
		mipLevelCount = mipMapGenerator->getMipLevelCount();
	}

	createImageInfo.format = imageFileLoaderFormat;
	createImageInfo.mipLevelCount = mipLevelCount;
	createImageInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
	m_image.reset(Wolf::Image::createImage(createImageInfo));
	m_image->copyCPUBuffer(imageFileLoader.getPixels(), Wolf::Image::SampledInFragmentShader());

	if (m_loadMips)
	{
		for (uint32_t mipLevel = 1; mipLevel < mipMapGenerator->getMipLevelCount(); ++mipLevel)
		{
			m_image->copyCPUBuffer(mipMapGenerator->getMipLevel(mipLevel).data(), Wolf::Image::SampledInFragmentShader(mipLevel), mipLevel);
		}
	}

	if (m_dataOnCPUStatus == DataOnCPUStatus::NOT_LOADED_YET)
	{
		m_firstMipData.resize(m_image->getBPP() * imageFileLoader.getWidth() * imageFileLoader.getHeight());
		memcpy(m_firstMipData.data(), imageFileLoader.getPixels(), m_firstMipData.size());

		m_dataOnCPUStatus = DataOnCPUStatus::AVAILABLE;
	}

	if (m_thumbnailGenerationRequested)
	{
		auto computeThumbnailData = [this, &imageFileLoader](std::vector<Wolf::ImageCompression::RGBA8>& out, float samplingZ = 0.0f)
		{
			bool hadAnErrorDuringGeneration = false;
			for (uint32_t x = 0; x < ThumbnailsGenerationPass::OUTPUT_SIZE; ++x)
			{
				for (uint32_t y = 0; y < ThumbnailsGenerationPass::OUTPUT_SIZE; ++y)
				{
					// We don't do bilinear here... Good enough?
					glm::vec3 samplingCoords = glm::vec3(static_cast<float>(x) / static_cast<float>(ThumbnailsGenerationPass::OUTPUT_SIZE), static_cast<float>(y) / static_cast<float>(ThumbnailsGenerationPass::OUTPUT_SIZE), samplingZ);
					glm::ivec3 samplingIndices = glm::ivec3(samplingCoords * glm::vec3(static_cast<float>(imageFileLoader.getWidth()), static_cast<float>(imageFileLoader.getHeight()), static_cast<float>(imageFileLoader.getDepth())));
					uint32_t samplingIndex = samplingIndices.x + samplingIndices.y * imageFileLoader.getWidth() + samplingIndices.z * imageFileLoader.getWidth() * imageFileLoader.getHeight();

					Wolf::ImageCompression::RGBA8& pixel = out[x + y * ThumbnailsGenerationPass::OUTPUT_SIZE];

					if (imageFileLoader.getFormat() == Wolf::Format::R32G32B32A32_SFLOAT)
					{
						glm::vec4* pixels = reinterpret_cast<glm::vec4*>(imageFileLoader.getPixels());
						pixel.r = glm::min(1.0f, pixels[samplingIndex].r) * 255.0f;
						pixel.g = glm::min(1.0f, pixels[samplingIndex].g) * 255.0f;
						pixel.b = glm::min(1.0f, pixels[samplingIndex].b) * 255.0f;
						pixel.a = 255.0f;
					}
					else if (imageFileLoader.getFormat() == Wolf::Format::R16G16B16A16_SFLOAT)
					{
						uint16_t* pixels = reinterpret_cast<uint16_t*>(imageFileLoader.getPixels());
						pixel.r = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex])) * 255.0f;
						pixel.g = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex + 1])) * 255.0f;
						pixel.b = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex + 2])) * 255.0f;
						pixel.a = 255.0f;
					}
					else if (imageFileLoader.getFormat() == Wolf::Format::R8G8B8A8_UNORM)
					{
						uint8_t* pixels = imageFileLoader.getPixels();
						pixel.r = pixels[4 * samplingIndex];
						pixel.g = pixels[4 * samplingIndex + 1];
						pixel.b = pixels[4 * samplingIndex + 2];
						pixel.a = pixels[4 * samplingIndex + 3];
					}
					else
					{
						Wolf::Debug::sendMessageOnce("Image format is not supported for thumbnail generation", Wolf::Debug::Severity::WARNING, this);
						hadAnErrorDuringGeneration = true;
					}
				}
			}

			return !hadAnErrorDuringGeneration;
		};

		std::string extension = fullFilePath.substr(fullFilePath.find_last_of('.') + 1);
		bool hadAnErrorDuringGeneration = false;

		if (extension == "cube")
		{
			GifEncoder gifEncoder;

			static constexpr int quality = 30;
			static constexpr bool useGlobalColorMap = false;
			static constexpr int loop = 0;
			static constexpr int preAllocSize = ThumbnailsGenerationPass::OUTPUT_SIZE * ThumbnailsGenerationPass::OUTPUT_SIZE * 3;

			if (!gifEncoder.open(computeIconPath(m_loadingPath, m_thumbnailCountToMaintain), ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE, quality, useGlobalColorMap, loop, preAllocSize))
			{
				Wolf::Debug::sendError("Error when opening gif file");
			}

			for (uint32_t z = 0; z < imageFileLoader.getDepth(); ++z)
			{
				std::vector<Wolf::ImageCompression::RGBA8> framePixels(ThumbnailsGenerationPass::OUTPUT_SIZE * ThumbnailsGenerationPass::OUTPUT_SIZE);
				if (!computeThumbnailData(framePixels, static_cast<float>(z) / static_cast<float>(imageFileLoader.getDepth())))
				{
					Wolf::Debug::sendMessageOnce("Error computing frame data for " + fullFilePath + " thumbnail generation", Wolf::Debug::Severity::ERROR, this);
					hadAnErrorDuringGeneration = true;
				}

				gifEncoder.push(GifEncoder::PIXEL_FORMAT_RGBA, reinterpret_cast<const uint8_t*>(framePixels.data()), ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE,
					static_cast<uint32_t>(ThumbnailsGenerationPass::DELAY_BETWEEN_ICON_FRAMES_MS) / 10);
			}

			gifEncoder.close();
		}
		else
		{
			std::vector<Wolf::ImageCompression::RGBA8> thumbnailPixels(ThumbnailsGenerationPass::OUTPUT_SIZE * ThumbnailsGenerationPass::OUTPUT_SIZE);

			if (computeThumbnailData(thumbnailPixels))
			{
				stbi_write_png(computeIconPath(m_loadingPath, m_thumbnailCountToMaintain).c_str(), ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE,
					4, thumbnailPixels.data(), ThumbnailsGenerationPass::OUTPUT_SIZE * sizeof(Wolf::ImageCompression::RGBA8));
			}
			else
			{
				hadAnErrorDuringGeneration = true;
			}
		}

		if (!hadAnErrorDuringGeneration)
		{
			m_updateAssetInUICallback(computeName(), computeIconPath(m_loadingPath, m_thumbnailCountToMaintain).substr(3, computeIconPath(m_loadingPath, m_thumbnailCountToMaintain).size()), m_resourceId);
		}
	}
}
