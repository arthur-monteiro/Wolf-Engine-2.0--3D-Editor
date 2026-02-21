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
#include "ImageFormatter.h"
#include "MeshAssetEditor.h"
#include "RenderMeshList.h"

AssetManager* AssetManager::ms_assetManager;

AssetManager::AssetManager(const std::function<void(const std::string&, const std::string&, AssetId)>& addAssetToUICallback, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback, 
                                 const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback,
                                 const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<void(const std::string& loadingPath)>& isolateMeshCallback,
                                 const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback, const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU)
	: m_addResourceToUICallback(addAssetToUICallback), m_updateResourceInUICallback(updateResourceInUICallback), m_requestReloadCallback(requestReloadCallback), m_editorConfiguration(editorConfiguration),
      m_materialsGPUManager(materialsGPUManager), m_thumbnailsGenerationPass(renderingPipeline->getThumbnailsGenerationPass()), m_isolateMeshCallback(isolateMeshCallback), m_removeIsolationAndGetViewMatrixCallback(removeIsolationAndGetViewMatrixCallback),
	  m_renderingPipeline(renderingPipeline), m_editorPushDataToGPU(editorPushDataToGPU)
{
	ms_assetManager = this;
}

void AssetManager::updateBeforeFrame()
{
	PROFILE_FUNCTION

	for (uint32_t i = 0; i < m_meshes.size(); ++i)
	{
		Wolf::ResourceUniqueOwner<Mesh>& mesh = m_meshes[i];
		mesh->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	for (uint32_t i = 0; i < m_images.size(); ++i)
	{
		Wolf::ResourceUniqueOwner<Image>& image = m_images[i];
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
			std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>>& physicsShapes = m_meshes[m_currentAssetInEdition]->getPhysicsShapes();
			physicsShapes.clear();

			for (uint32_t i = 0; i < m_meshAssetEditor->getPhysicsMeshCount(); ++i)
			{
				Wolf::Physics::PhysicsShapeType shapeType = m_meshAssetEditor->getShapeType(i);

				if (shapeType == Wolf::Physics::PhysicsShapeType::Rectangle)
				{
					glm::vec3 p0;
					glm::vec3 s1;
					glm::vec3 s2;
					m_meshAssetEditor->computeInfoForRectangle(i, p0, s1, s2);

					physicsShapes.emplace_back(new Wolf::Physics::Rectangle(m_meshAssetEditor->getShapeName(i), p0, s1, s2));
				}
				else if (shapeType == Wolf::Physics::PhysicsShapeType::Box)
				{
					glm::vec3 aabbMin;
					glm::vec3 aabbMax;
					glm::vec3 rotation;
					m_meshAssetEditor->computeInfoForBox(i, aabbMin, aabbMax, rotation);

					physicsShapes.emplace_back(new Wolf::Physics::Box(m_meshAssetEditor->getShapeName(i), aabbMin, aabbMax, rotation));
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
	m_combinedImages.clear();
}

void AssetManager::releaseRenderingPipeline()
{
	m_thumbnailsGenerationPass.release();
	m_renderingPipeline.release();
}

void AssetManager::dump(const std::string& dumpLocalFolder)
{
	std::ofstream outputJSON;
	outputJSON.open(m_editorConfiguration->computeFullPathFromLocalPath(dumpLocalFolder + "/assets.json"));

	outputJSON << "{" << std::endl;
	outputJSON << "\t\"meshes\": [" << std::endl;
	for (uint32_t i = 0; i < m_meshes.size(); ++i)
	{
		outputJSON << "\t\t{" << std::endl;
		m_meshes[i]->dump(outputJSON, 3, m_editorConfiguration->computeFullPathFromLocalPath(dumpLocalFolder));
		outputJSON << "\t\t}";

		if (i != m_meshes.size() - 1)
		{
			outputJSON << ",";
		}
		outputJSON << std::endl;
	}
	outputJSON << "\t]," << std::endl;
	outputJSON << "\t\"images\": [" << std::endl;
	for (uint32_t i = 0; i < m_images.size(); ++i)
	{
		outputJSON << "\t\t{" << std::endl;
		m_images[i]->dump(outputJSON, 3, m_editorConfiguration->computeFullPathFromLocalPath(dumpLocalFolder));
		outputJSON << "\t\t}";

		if (i != m_images.size() - 1)
		{
			outputJSON << ",";
		}
		outputJSON << std::endl;
	}
	outputJSON << "\t]," << std::endl;
	outputJSON << "\t\"combinedImages\": [" << std::endl;
	for (uint32_t i = 0; i < m_combinedImages.size(); ++i)
	{
		outputJSON << "\t\t{" << std::endl;
		m_combinedImages[i]->dump(outputJSON, 3, m_editorConfiguration->computeFullPathFromLocalPath(dumpLocalFolder));
		outputJSON << "\t\t}";

		if (i != m_combinedImages.size() - 1)
		{
			outputJSON << ",";
		}
		outputJSON << std::endl;
	}
	outputJSON << "\t]" << std::endl;
	outputJSON << "}" << std::endl;

	outputJSON.close();
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
			uint32_t firstMaterialIdx = getFirstMaterialIdx(assetId);
			Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> bottomLevelAccelerationStructure = getBLAS(assetId, 0, 0);
			meshResourceEditor = new MeshAssetEditor(m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getLoadingPath(), m_requestReloadCallback, isModelCentered(assetId), getModelMesh(assetId), getModelDefaultLODInfo(assetId),
				getModelSloppyLODInfo(assetId), getModelDefaultSimplifiedIndexBuffers(assetId), getModelSloppySimplifiedIndexBuffers(assetId), firstMaterialIdx,
				bottomLevelAccelerationStructure, m_isolateMeshCallback, m_removeIsolationAndGetViewMatrixCallback,
				[this, assetId](const glm::mat4& viewMatrix) { requestThumbnailReload(assetId, viewMatrix); }, m_renderingPipeline, m_editorPushDataToGPU);

			for (Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& shape : getPhysicsShapes(assetId))
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

bool AssetManager::isCombinedImage(AssetId assetId)
{
	return assetId == NO_ASSET || (assetId >= COMBINED_IMAGE_ASSET_IDX_OFFSET && assetId < COMBINED_IMAGE_ASSET_IDX_OFFSET + MAX_COMBINED_IMAGE_ASSET_COUNT);
}

AssetId AssetManager::addModel(const std::string& loadingPath)
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

Wolf::ResourceNonOwner<Wolf::Mesh> AssetManager::getModelMesh(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getMesh();
}

bool AssetManager::isModelCentered(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->isCentered();
}

const std::vector<ModelData::LODInfo>& AssetManager::getModelDefaultLODInfo(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getDefaultLODInfo();
}

const std::vector<ModelData::LODInfo>& AssetManager::getModelSloppyLODInfo(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getSloppyLODInfo();
}

std::vector<Wolf::ResourceNonOwner<Wolf::Buffer>> AssetManager::getModelDefaultSimplifiedIndexBuffers(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getDefaultSimplifiedIndexBuffers();
}

std::vector<Wolf::ResourceNonOwner<Wolf::Buffer>> AssetManager::getModelSloppySimplifiedIndexBuffers(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getSloppySimplifiedIndexBuffers();
}

Wolf::ResourceNonOwner<AnimationData> AssetManager::getAnimationData(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getAnimationData();
}

Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> AssetManager::getBLAS(AssetId modelResourceId, uint32_t lod, uint32_t lodType)
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getBLAS(lod, lodType);
}

std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>>& AssetManager::getPhysicsShapes(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getPhysicsShapes();
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

const std::vector<Wolf::MaterialsGPUManager::TextureSetInfo>& AssetManager::getModelTextureSetInfo(AssetId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_ASSET_IDX_OFFSET]->getTextureSetInfo();
}

void AssetManager::subscribeToMesh(AssetId assetId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->subscribe(instance, callback);
}

AssetId AssetManager::addImage(const std::string& loadingPath, bool loadMips, Wolf::Format format, bool keepDataOnCPU, bool canBeVirtualized, bool forceImmediateLoading)
{
	if (m_images.size() >= MAX_IMAGE_ASSET_COUNT)
	{
		Wolf::Debug::sendCriticalError("Maximum image resources reached");
		return NO_ASSET;
	}

	AssetId assetId = NO_ASSET;

	for (uint32_t i = 0; i < m_images.size(); ++i)
	{
		if (m_images[i]->getLoadingPath() == loadingPath)
		{
			assetId = i + IMAGE_ASSET_IDX_OFFSET;
			break;
		}
	}

	if (assetId == NO_ASSET)
	{
		assetId = static_cast<uint32_t>(m_images.size()) + IMAGE_ASSET_IDX_OFFSET;

		std::string iconFullPath = computeIconPath(loadingPath, 0);
		bool iconFileExists = formatIconPath(loadingPath, iconFullPath);

		m_images.emplace_back(new Image(m_editorPushDataToGPU, loadingPath, !iconFileExists, assetId, m_updateResourceInUICallback, loadMips, format, keepDataOnCPU, canBeVirtualized));
		m_addResourceToUICallback(m_images.back()->computeName(), iconFullPath, assetId);
	}

	if (forceImmediateLoading)
	{
		m_images[assetId - IMAGE_ASSET_IDX_OFFSET]->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	return assetId;
}

bool AssetManager::isImageLoaded(AssetId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}

	return m_images[imageResourceId - IMAGE_ASSET_IDX_OFFSET]->isLoaded();
}

Wolf::ResourceNonOwner<Wolf::Image> AssetManager::getImage(AssetId imageAssetId) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->getImage();
}

const uint8_t* AssetManager::getImageData(AssetId imageAssetId) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->getFirstMipData();
}

void AssetManager::deleteImageData(AssetId imageAssetId) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->deleteImageData();
}

std::string AssetManager::getImageSlicesFolder(AssetId imageAssetId) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->getSlicesFolder();
}

AssetId AssetManager::addCombinedImage(const std::string& loadingPathR, const std::string& loadingPathG, const std::string& loadingPathB, const std::string& loadingPathA, bool loadMips, Wolf::Format format,
                                       bool keepDataOnCPU, bool canBeVirtualized, bool forceImmediateLoading)
{
	if (m_combinedImages.size() >= MAX_COMBINED_IMAGE_ASSET_COUNT)
	{
		Wolf::Debug::sendCriticalError("Maximum combined image resources reached");
		return NO_ASSET;
	}

	std::string combinedLoadingPath = extractFolderFromFullPath(loadingPathR) + "combined_";
	combinedLoadingPath += extractFilenameFromPath(loadingPathR) + "_";
	combinedLoadingPath += extractFilenameFromPath(loadingPathG) + "_";
	combinedLoadingPath += extractFilenameFromPath(loadingPathB) + "_";
	combinedLoadingPath += extractFilenameFromPath(loadingPathA);

	AssetId assetId = NO_ASSET;

	for (uint32_t i = 0; i < m_combinedImages.size(); ++i)
	{
		if (m_combinedImages[i]->getLoadingPath() == combinedLoadingPath)
		{
			assetId = i + COMBINED_IMAGE_ASSET_IDX_OFFSET;
			break;
		}
	}

	if (assetId == NO_ASSET)
	{
		assetId = static_cast<uint32_t>(m_combinedImages.size()) + COMBINED_IMAGE_ASSET_IDX_OFFSET;

		std::string iconFullPath = computeIconPath(combinedLoadingPath, 0);
		bool iconFileExists = formatIconPath(combinedLoadingPath, iconFullPath);

		m_combinedImages.emplace_back(new CombinedImage(m_editorPushDataToGPU, combinedLoadingPath, loadingPathR, loadingPathG, loadingPathB, loadingPathA, !iconFileExists, assetId, m_updateResourceInUICallback,
			loadMips, format, keepDataOnCPU, canBeVirtualized));
		m_addResourceToUICallback(m_combinedImages.back()->computeName(), iconFullPath, assetId);
	}

	if (forceImmediateLoading)
	{
		m_combinedImages[assetId - COMBINED_IMAGE_ASSET_IDX_OFFSET]->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	return assetId;
}

Wolf::ResourceNonOwner<Wolf::Image> AssetManager::getCombinedImage(AssetId combinedImageAssetId) const
{
	if (!isCombinedImage(combinedImageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not a combined image");
	}

	return m_combinedImages[combinedImageAssetId - COMBINED_IMAGE_ASSET_IDX_OFFSET]->getImage();
}

std::string AssetManager::getCombinedImageSlicesFolder(AssetId combinedImageAssetId) const
{
	if (!isCombinedImage(combinedImageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not a combined image");
	}

	return m_combinedImages[combinedImageAssetId - COMBINED_IMAGE_ASSET_IDX_OFFSET]->getSlicesFolder();
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

std::string AssetManager::extractFilenameFromPath(const std::string& fullPath)
{
	return fullPath.substr(fullPath.find_last_of("/\\") + 1);
}

std::string AssetManager::extractFolderFromFullPath(const std::string& fullPath)
{
	return fullPath.substr(0, fullPath.find_last_of("/\\"));
}

AssetManager::AssetInterface::AssetInterface(std::string loadingPath, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback)
	: m_loadingPath(std::move(loadingPath)), m_assetId(assetId), m_updateAssetInUICallback(updateResourceInUICallback)
{
}

void AssetManager::AssetInterface::dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath)
{
	auto insertTabs = [&outputJSON, tabCount]()
	{
		for (uint32_t i = 0; i < tabCount; ++i)
			outputJSON << "\t";
	};

	insertTabs();
	outputJSON << "\"loadingPath\": \"" + m_loadingPath + "\",\n";
	insertTabs();
	outputJSON << "\"assetId\":" + std::to_string(m_assetId);
}

std::string AssetManager::AssetInterface::computeName()
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

void AssetManager::AssetInterface::onChanged()
{
	notifySubscribers();
}

AssetManager::Mesh::Mesh(const std::string& loadingPath, bool needThumbnailsGeneration, AssetId resourceId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback)
	: AssetInterface(loadingPath, resourceId, updateResourceInUICallback)
{
	m_modelLoadingRequested = true;
	m_thumbnailGenerationRequested = !g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration;
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
	m_meshToKeepInMemory.reset(m_mesh.release());

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

void AssetManager::Mesh::dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath)
{
	AssetInterface::dump(outputJSON, tabCount, folderPath);
	outputJSON << std::endl;

	ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = m_loadingPath;
	modelLoadingInfo.mtlFolder = m_loadingPath.substr(0, m_loadingPath.find_last_of('/'));
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.textureSetLayout = TextureSetLoader::InputTextureSetLayout::EACH_TEXTURE_A_FILE;
	modelLoadingInfo.generateDefaultLODCount = 8;
	modelLoadingInfo.generateSloppyLODCount= 16;

	ModelData modelData;
	ModelLoader::loadObject(modelData, modelLoadingInfo, Wolf::ResourceReference<AssetManager>(ms_assetManager));

	std::string binFilename = folderPath + "/" + std::to_string(m_assetId) + ".bin";
	std::ofstream fileStream(binFilename.c_str(), std::ios::out | std::ios::binary);

	uint8_t isAnimated = modelData.m_skeletonVertices.empty() ? 0u : 1u;
	fileStream.write(reinterpret_cast<const char*>(&isAnimated), sizeof(isAnimated));

	uint32_t vertexCount = isAnimated ? static_cast<uint32_t>(modelData.m_skeletonVertices.size()) :static_cast<uint32_t>(modelData.m_staticVertices.size());
	fileStream.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
	if (isAnimated)
	{
		fileStream.write(reinterpret_cast<const char*>(modelData.m_skeletonVertices.data()), sizeof(modelData.m_skeletonVertices[0]) * vertexCount);
	}
	else
	{
		fileStream.write(reinterpret_cast<const char*>(modelData.m_staticVertices.data()), sizeof(modelData.m_staticVertices[0]) * vertexCount);
	}

	uint32_t indexCount = modelData.m_indices.size();
	fileStream.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
	fileStream.write(reinterpret_cast<const char*>(modelData.m_indices.data()), sizeof(modelData.m_indices[0]) * indexCount);

	uint32_t textureSetCount = m_textureSetInfo.size();
	fileStream.write(reinterpret_cast<const char*>(&textureSetCount), sizeof(textureSetCount));
	std::vector<AssetId> textureSetsAssetId(textureSetCount * Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL);
	for (uint32_t i = 0; i < textureSetCount; i++)
	{
		for (uint32_t j = 0; j < Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL; j++)
		{
			textureSetsAssetId[i * Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL + j] = m_textureSetInfo[i].imageAssetIds[j];
		}
	}
	fileStream.write(reinterpret_cast<const char*>(textureSetsAssetId.data()), sizeof(textureSetsAssetId[0]) * textureSetsAssetId.size());

	fileStream.close();
}

bool AssetManager::Mesh::isLoaded() const
{
	return static_cast<bool>(m_mesh);
}

std::vector<Wolf::ResourceNonOwner<Wolf::Buffer>> AssetManager::Mesh::getDefaultSimplifiedIndexBuffers()
{
	std::vector<Wolf::ResourceNonOwner<Wolf::Buffer>> r;
	for (uint32_t i = 0; i < m_defaultSimplifiedIndexBuffers.size(); i++)
	{
		r.emplace_back(m_defaultSimplifiedIndexBuffers[i].createNonOwnerResource());
	}
	return r;
}

std::vector<Wolf::ResourceNonOwner<Wolf::Buffer>> AssetManager::Mesh::getSloppySimplifiedIndexBuffers()
{
	std::vector<Wolf::ResourceNonOwner<Wolf::Buffer>> r;
	for (uint32_t i = 0; i < m_sloppySimplifiedIndexBuffers.size(); i++)
	{
		r.emplace_back(m_sloppySimplifiedIndexBuffers[i].createNonOwnerResource());
	}
	return r;
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
	Wolf::Timer timer(std::string(m_loadingPath) + " loading");

	ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = m_loadingPath;
	modelLoadingInfo.mtlFolder = m_loadingPath.substr(0, m_loadingPath.find_last_of('/'));
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.textureSetLayout = TextureSetLoader::InputTextureSetLayout::EACH_TEXTURE_A_FILE;
	modelLoadingInfo.generateDefaultLODCount = 8;
	modelLoadingInfo.generateSloppyLODCount= 16;

	ModelData modelData;
	ModelLoader::loadObject(modelData, modelLoadingInfo, Wolf::ResourceReference<AssetManager>(ms_assetManager));

	VkBufferUsageFlags additionalFlags = 0;
	if (g_editorConfiguration->getEnableRayTracing())
	{
		additionalFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}

	if (!modelData.m_staticVertices.empty())
	{
		m_mesh.reset(new Wolf::Mesh(modelData.m_staticVertices, modelData.m_indices, modelData.m_aabb, modelData.m_boundingSphere, additionalFlags, additionalFlags));
	}
	else if (!modelData.m_skeletonVertices.empty())
	{
		m_mesh.reset(new Wolf::Mesh(modelData.m_skeletonVertices, modelData.m_indices, modelData.m_aabb, modelData.m_boundingSphere, additionalFlags, additionalFlags));
	}
	else
	{
		Wolf::Debug::sendCriticalError("No vertex found");
	}

	m_isCentered = modelData.m_isMeshCentered;

	for (const std::vector<uint32_t>& lod : modelData.m_defaultSimplifiedIndices)
	{
		m_defaultSimplifiedIndexBuffers.emplace_back(Wolf::Buffer::createBuffer(lod.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | additionalFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
		m_defaultSimplifiedIndexBuffers.back()->transferCPUMemoryWithStagingBuffer(lod.data(), lod.size() * sizeof(uint32_t));
	}

	for (const std::vector<uint32_t>& lod : modelData.m_sloppySimplifiedIndices)
	{
		m_sloppySimplifiedIndexBuffers.emplace_back(Wolf::Buffer::createBuffer(lod.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | additionalFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
		m_sloppySimplifiedIndexBuffers.back()->transferCPUMemoryWithStagingBuffer(lod.data(), lod.size() * sizeof(uint32_t));
	}

	m_defaultLODsInfo = modelData.m_defaultLODsInfo;
	m_sloppyLODsInfo = modelData.m_sloppyLODsInfo;

	if (modelData.m_animationData)
	{
		m_animationData = modelData.m_animationData.release();
	}

	m_physicsShapes.resize(modelData.m_physicsShapes.size());
	for (uint32_t i = 0; i < modelData.m_physicsShapes.size(); ++i)
	{
		m_physicsShapes[i] = modelData.m_physicsShapes[i].release();
	}

	materialsGPUManager->lockMaterials();
	materialsGPUManager->lockTextureSets();

	m_firstTextureSetIdx = modelData.m_textureSets.empty() ? 0 : materialsGPUManager->getCurrentTextureSetCount();
	m_firstMaterialIdx = modelData.m_textureSets.empty() ? 0 : materialsGPUManager->getCurrentMaterialCount();
	for (const Wolf::MaterialsGPUManager::TextureSetInfo& textureSetInfo : modelData.m_textureSets)
	{
		materialsGPUManager->addNewTextureSet(textureSetInfo);

		Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
		materialInfo.textureSetInfos[0].textureSetIdx = materialsGPUManager->getTextureSetsCacheInfo().back().textureSetIdx;
		materialsGPUManager->addNewMaterial(materialInfo);
	}
	materialsGPUManager->unlockTextureSets();
	materialsGPUManager->unlockMaterials();

	m_textureSetInfo = modelData.m_textureSets;

	if (g_editorConfiguration->getEnableRayTracing())
	{
		m_bottomLevelAccelerationStructures.resize(2);
		for (uint32_t lodType = 0; lodType < 2; lodType++)
		{
			uint32_t lodCount = lodType == 0 ? m_defaultSimplifiedIndexBuffers.size() : m_sloppySimplifiedIndexBuffers.size();

			m_bottomLevelAccelerationStructures[lodType].resize(lodCount + 1);
			for (uint32_t lod = 0; lod < m_bottomLevelAccelerationStructures[lodType].size(); lod++)
			{
				buildBLAS(lod, lodType);
			}
		}
	}

	Wolf::AABB entityAABB = modelData.m_aabb;
	float entityHeight = entityAABB.getMax().y - entityAABB.getMin().y;
	glm::vec3 position = entityAABB.getCenter() + glm::vec3(-entityHeight, entityHeight, -entityHeight);
	glm::vec3 target = entityAABB.getCenter();
	m_thumbnailGenerationViewMatrix = glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void AssetManager::Mesh::generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	std::string iconPath = computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);

	thumbnailsGenerationPass->addRequestBeforeFrame({ getMesh(), isAnimated() ? Wolf::NullableResourceNonOwner<AnimationData>(getAnimationData()) : Wolf::NullableResourceNonOwner<AnimationData>(), m_firstMaterialIdx, iconPath,
		[this, iconPath]() { m_updateAssetInUICallback(computeName(), iconPath.substr(3, iconPath.size()), m_assetId); },
			m_thumbnailGenerationViewMatrix });
}

void AssetManager::Mesh::buildBLAS(uint32_t lod, uint32_t lodType)
{
	Wolf::GeometryInfo geometryInfo;
	geometryInfo.mesh.vertexBuffer = &*m_mesh->getVertexBuffer();
	geometryInfo.mesh.vertexCount = m_mesh->getVertexCount();
	geometryInfo.mesh.vertexSize = m_mesh->getVertexSize();
	geometryInfo.mesh.vertexFormat = Wolf::Format::R32G32B32_SFLOAT;
	if (lod == 0)
	{
		geometryInfo.mesh.indexBuffer = &*m_mesh->getIndexBuffer();
		geometryInfo.mesh.indexCount = m_mesh->getIndexCount();
	}
	else
	{
		Wolf::ResourceUniqueOwner<Wolf::Buffer>* overrideIndexBuffer = nullptr;
		if (lodType == 0)
		{
			overrideIndexBuffer = &m_defaultSimplifiedIndexBuffers[lod - 1];
		}
		else
		{
			overrideIndexBuffer = &m_sloppySimplifiedIndexBuffers[lod - 1];
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

void AssetManager::ImageInterface::deleteImageData()
{
	m_firstMipData.clear();
	m_dataOnCPUStatus = DataOnCPUStatus::DELETED;
}

bool AssetManager::ImageInterface::generateThumbnail(const std::string& fullFilePath, const std::string& iconPath)
{
	auto computeThumbnailData = [this](std::vector<Wolf::ImageCompression::RGBA8>& out, float samplingZ = 0.0f)
	{
		Wolf::Extent3D imageExtent = m_image->getExtent();

		std::vector<Wolf::ImageCompression::RGBA8> RGBA8Pixels;
		std::vector<Wolf::ImageCompression::RG8> RG8Pixels;
		if (m_image->getFormat() == Wolf::Format::BC1_RGB_SRGB_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC1, m_firstMipData.data(), { imageExtent.width, imageExtent.height }, RGBA8Pixels);
		}
		else if (m_image->getFormat() == Wolf::Format::BC3_UNORM_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC3, m_firstMipData.data(), { imageExtent.width, imageExtent.height }, RGBA8Pixels);
		}
		else if (m_image->getFormat() == Wolf::Format::BC5_UNORM_BLOCK)
		{
			Wolf::ImageCompression::uncompressImage(Wolf::ImageCompression::Compression::BC5, m_firstMipData.data(), { imageExtent.width, imageExtent.height }, RG8Pixels);
		}

		bool hadAnErrorDuringGeneration = false;
		for (uint32_t x = 0; x < ThumbnailsGenerationPass::OUTPUT_SIZE; ++x)
		{
			for (uint32_t y = 0; y < ThumbnailsGenerationPass::OUTPUT_SIZE; ++y)
			{
				// We don't do bilinear here... Good enough?
				glm::vec3 samplingCoords = glm::vec3(static_cast<float>(x) / static_cast<float>(ThumbnailsGenerationPass::OUTPUT_SIZE), static_cast<float>(y) / static_cast<float>(ThumbnailsGenerationPass::OUTPUT_SIZE), samplingZ);
				glm::ivec3 samplingIndices = glm::ivec3(samplingCoords * glm::vec3(static_cast<float>(imageExtent.width), static_cast<float>(imageExtent.height), static_cast<float>(imageExtent.depth)));
				uint32_t samplingIndex = samplingIndices.x + samplingIndices.y * imageExtent.width + samplingIndices.z * imageExtent.width * imageExtent.height;

				Wolf::ImageCompression::RGBA8& pixel = out[x + y * ThumbnailsGenerationPass::OUTPUT_SIZE];

				if (m_image->getFormat() == Wolf::Format::R32G32B32A32_SFLOAT)
				{
					glm::vec4* pixels = reinterpret_cast<glm::vec4*>(m_firstMipData.data());
					pixel.r = glm::min(1.0f, pixels[samplingIndex].r) * 255.0f;
					pixel.g = glm::min(1.0f, pixels[samplingIndex].g) * 255.0f;
					pixel.b = glm::min(1.0f, pixels[samplingIndex].b) * 255.0f;
					pixel.a = 255.0f;
				}
				else if (m_image->getFormat() == Wolf::Format::R16G16B16A16_SFLOAT)
				{
					uint16_t* pixels = reinterpret_cast<uint16_t*>(m_firstMipData.data());
					pixel.r = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex])) * 255.0f;
					pixel.g = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex + 1])) * 255.0f;
					pixel.b = glm::min(1.0f, glm::unpackHalf1x16(pixels[4 * samplingIndex + 2])) * 255.0f;
					pixel.a = 255.0f;
				}
				else if (m_image->getFormat() == Wolf::Format::R8G8B8A8_UNORM)
				{
					uint8_t* pixels = m_firstMipData.data();
					pixel.r = pixels[4 * samplingIndex];
					pixel.g = pixels[4 * samplingIndex + 1];
					pixel.b = pixels[4 * samplingIndex + 2];
					pixel.a = pixels[4 * samplingIndex + 3];
				}
				else if (m_image->getFormat() == Wolf::Format::BC1_RGB_SRGB_BLOCK || m_image->getFormat() == Wolf::Format::BC3_UNORM_BLOCK)
				{
					pixel = RGBA8Pixels[samplingIndex];
					pixel.a = 255;
				}
				else if (m_image->getFormat() == Wolf::Format::BC5_UNORM_BLOCK)
				{
					pixel.r = RG8Pixels[samplingIndex].r;
					pixel.g = RG8Pixels[samplingIndex].g;
					pixel.b = 0;
					pixel.a = 255;
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

		if (!gifEncoder.open(iconPath, ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE, quality, useGlobalColorMap, loop, preAllocSize))
		{
			Wolf::Debug::sendError("Error when opening gif file");
		}

		for (uint32_t z = 0; z < m_image->getExtent().depth; ++z)
		{
			std::vector<Wolf::ImageCompression::RGBA8> framePixels(ThumbnailsGenerationPass::OUTPUT_SIZE * ThumbnailsGenerationPass::OUTPUT_SIZE);
			if (!computeThumbnailData(framePixels, static_cast<float>(z) / static_cast<float>(m_image->getExtent().depth)))
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
			stbi_write_png(iconPath.c_str(), ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE,
				4, thumbnailPixels.data(), ThumbnailsGenerationPass::OUTPUT_SIZE * sizeof(Wolf::ImageCompression::RGBA8));
		}
		else
		{
			hadAnErrorDuringGeneration = true;
		}
	}

	return !hadAnErrorDuringGeneration;
}

void AssetManager::ImageInterface::dump(const std::string& folderPath, AssetId assetId, ImageFormatter& imageFormatter) const
{
	std::string binFilename = folderPath + "/" + std::to_string(assetId) + ".bin";
	std::ofstream fileStream(binFilename.c_str(), std::ios::out | std::ios::binary);

	fileStream.write(reinterpret_cast<const char*>(&m_format), sizeof(m_format));

	Wolf::Extent3D extent = m_image->getExtent();
	fileStream.write(reinterpret_cast<const char*>(&extent), sizeof(extent));

	uint32_t pixelCount = extent.width * extent.height * extent.depth;
	fileStream.write(reinterpret_cast<const char*>(imageFormatter.getPixels()), pixelCount * m_image->getBPP());

	uint32_t mipCount = imageFormatter.getMipCountKeptOnCPU();
	fileStream.write(reinterpret_cast<const char*>(&mipCount), sizeof(mipCount));

	for (uint32_t mipLevel = 1; mipLevel < mipCount + 1; ++mipLevel)
	{
		const Wolf::Extent3D mipExtent = { extent.width >> mipLevel, extent.height >> mipLevel, extent.depth };
		pixelCount = mipExtent.width * mipExtent.height * mipExtent.depth;
		fileStream.write(reinterpret_cast<const char*>(imageFormatter.getPixels(mipLevel)), pixelCount * m_image->getBPP());
	}

	fileStream.close();
}

AssetManager::Image::Image(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId,
                           const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback, bool loadMips, Wolf::Format format, bool keepDataOnCPU, bool canBeVirtualized)
	: AssetInterface(loadingPath, assetId, updateResourceInUICallback), ImageInterface(editorPushDataToGPU, needThumbnailsGeneration, loadMips, format, canBeVirtualized)
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

void AssetManager::Image::dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath)
{
	AssetInterface::dump(outputJSON, tabCount, folderPath);
	outputJSON << std::endl;

	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath);
	ImageFormatter imageFormatter(m_editorPushDataToGPU, fullFilePath, "", m_format, m_canBeVirtualized, ImageFormatter::KeepDataMode::ONLY_CPU, m_loadMips);

	ImageInterface::dump(folderPath, m_assetId, imageFormatter);
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

void AssetManager::Image::loadImage()
{
	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath);

	bool keepDataOnCPU = m_thumbnailGenerationRequested || m_dataOnCPUStatus == DataOnCPUStatus::NOT_LOADED_YET;
	ImageFormatter imageFormatter(m_editorPushDataToGPU, fullFilePath, "", m_format, m_canBeVirtualized, keepDataOnCPU ? ImageFormatter::KeepDataMode::CPU_AND_GPU : ImageFormatter::KeepDataMode::ONLY_GPU, m_loadMips);
	imageFormatter.transferImageTo(m_image);
	m_slicesFolder = imageFormatter.getSlicesFolder();

	if (m_thumbnailGenerationRequested || m_dataOnCPUStatus == DataOnCPUStatus::NOT_LOADED_YET)
	{
		m_firstMipData.resize(m_image->getBPP() * m_image->getExtent().width * m_image->getExtent().height * m_image->getExtent().depth);
		memcpy(m_firstMipData.data(), imageFormatter.getPixels(), m_firstMipData.size());

		if (m_dataOnCPUStatus == DataOnCPUStatus::NOT_LOADED_YET)
		{
			m_dataOnCPUStatus = DataOnCPUStatus::AVAILABLE;
		}
	}

	if (m_thumbnailGenerationRequested)
	{
		std::string iconPath = computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);

		if (generateThumbnail(fullFilePath, iconPath))
		{
			m_updateAssetInUICallback(computeName(), iconPath.substr(3, iconPath.size()), m_assetId);
		}

		if (m_dataOnCPUStatus != DataOnCPUStatus::AVAILABLE)
		{
			deleteImageData();
		}
	}
}

AssetManager::CombinedImage::CombinedImage(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const std::string& combinedLoadingPath, const std::string& loadingPathR, const std::string& loadingPathG, const std::string& loadingPathB, const std::string& loadingPathA, bool needThumbnailsGeneration,
	AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback, bool loadMips, Wolf::Format format, bool keepDataOnCPU, bool canBeVirtualized)
 : AssetInterface(combinedLoadingPath, assetId, updateResourceInUICallback), ImageInterface(editorPushDataToGPU, needThumbnailsGeneration, loadMips, format, canBeVirtualized)
{
	m_imageLoadingRequested = true;
	m_dataOnCPUStatus = keepDataOnCPU ? DataOnCPUStatus::NOT_LOADED_YET : DataOnCPUStatus::NEVER_KEPT;

	m_loadingPaths[0] = loadingPathR;
	m_loadingPaths[1] = loadingPathG;
	m_loadingPaths[2] = loadingPathB;
	m_loadingPaths[3] = loadingPathA;

	if (loadingPathR.empty())
	{
		Wolf::Debug::sendCriticalError("R component can't be empty");
	}

	if (m_format != Wolf::Format::BC3_UNORM_BLOCK)
	{
		Wolf::Debug::sendCriticalError("Format unsupported");
	}
}

void AssetManager::CombinedImage::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	if (m_imageLoadingRequested)
	{
		loadImage();
		if (m_thumbnailGenerationRequested)
		{
			// TODO
		}
		m_imageLoadingRequested = false;
	}
}

bool AssetManager::CombinedImage::isLoaded() const
{
	return static_cast<bool>(m_image);
}

void AssetManager::CombinedImage::dump(std::ofstream& outputJSON, uint32_t tabCount, const std::string& folderPath)
{
	AssetInterface::dump(outputJSON, tabCount, folderPath);
	outputJSON << std::endl;
}

void AssetManager::CombinedImage::loadImage()
{
	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath);

	std::string filenameNoExtensionR = m_loadingPaths[0].substr(m_loadingPaths[0].find_last_of("\\") + 1, m_loadingPaths[0].find_last_of("."));
	std::string filenameNoExtensionG = m_loadingPaths[1].substr(m_loadingPaths[1].find_last_of("\\") + 1, m_loadingPaths[1].find_last_of("."));
	std::string filenameNoExtensionB = m_loadingPaths[2].substr(m_loadingPaths[2].find_last_of("\\") + 1, m_loadingPaths[2].find_last_of("."));
	std::string filenameNoExtensionA = m_loadingPaths[3].substr(m_loadingPaths[3].find_last_of("\\") + 1, m_loadingPaths[3].find_last_of("."));
	std::string folder = m_loadingPaths[0].substr(0, m_loadingPaths[0].find_last_of("\\"));
	std::string slicesFolder = g_editorConfiguration->computeFullPathFromLocalPath(folder + '\\' + filenameNoExtensionR + "_" + filenameNoExtensionG + "_" + filenameNoExtensionB + "_" + filenameNoExtensionA + "_bin" + "\\");

	bool keepDataOnCPU = m_thumbnailGenerationRequested || m_dataOnCPUStatus == DataOnCPUStatus::NOT_LOADED_YET;

	if (!keepDataOnCPU && ImageFormatter::isCacheAvailable(fullFilePath, slicesFolder, m_canBeVirtualized))
	{
		ImageFormatter imageFormatter(m_editorPushDataToGPU,fullFilePath, slicesFolder, m_format, m_canBeVirtualized, ImageFormatter::KeepDataMode::ONLY_GPU, m_loadMips);
		imageFormatter.transferImageTo(m_image);
		m_slicesFolder = imageFormatter.getSlicesFolder();
		return;
	}

	std::vector<Wolf::ImageCompression::RGBA8> combinedData;
	std::vector<std::vector<Wolf::ImageCompression::RGBA8>> combinedMipLevels;
	Wolf::Extent3D combinedExtent = { 1, 1, 1};
	combinedData.resize(1);

	for (uint32_t channelIdx = 0; channelIdx < 4; channelIdx++)
	{
		if (m_loadingPaths[channelIdx].empty())
			continue;

		std::string channelFullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPaths[channelIdx]);

		std::vector<Wolf::ImageCompression::RGBA8> pixels;
		std::vector<std::vector<Wolf::ImageCompression::RGBA8>> mipLevels;
		Wolf::Extent3D extent;
		ImageFormatter::loadImageFile(channelFullFilePath, Wolf::Format::R8G8B8A8_UNORM, true, pixels, mipLevels, extent);

		if (channelIdx == 0)
		{
			combinedData.resize(pixels.size());
			combinedExtent = extent;
		}
		if (extent != combinedExtent)
		{
			Wolf::Debug::sendCriticalError("Extent of all channels must be the same");
		}
		for (uint32_t pixelIdx = 0; pixelIdx < pixels.size(); ++pixelIdx)
		{
			combinedData[pixelIdx][channelIdx] = pixels[pixelIdx][channelIdx];
		}

		if (channelIdx == 0)
		{
			combinedMipLevels.resize(mipLevels.size());
		}
		if (combinedMipLevels.size() != mipLevels.size())
		{
			Wolf::Debug::sendCriticalError("MipLevels of all channels must be the same");
		}
		for (uint32_t mipLevel = 0; mipLevel < mipLevels.size(); ++mipLevel)
		{
			if (channelIdx == 0)
			{
				combinedMipLevels[mipLevel].resize(mipLevels[mipLevel].size());
			}

			for (uint32_t pixelIdx = 0; pixelIdx < mipLevels[mipLevel].size(); ++pixelIdx)
			{
				combinedMipLevels[mipLevel][pixelIdx].r = mipLevels[mipLevel][pixelIdx].r;
			}
		}
	}

	ImageFormatter imageFormatter(m_editorPushDataToGPU, combinedData, combinedMipLevels, combinedExtent, slicesFolder, fullFilePath, m_format, m_canBeVirtualized, keepDataOnCPU ? ImageFormatter::KeepDataMode::CPU_AND_GPU : ImageFormatter::KeepDataMode::ONLY_GPU);
	imageFormatter.transferImageTo(m_image);
	m_slicesFolder = imageFormatter.getSlicesFolder();

	if (keepDataOnCPU)
	{
		m_firstMipData.resize(m_image->getBPP() * m_image->getExtent().width * m_image->getExtent().height * m_image->getExtent().depth);
		memcpy(m_firstMipData.data(), imageFormatter.getPixels(), m_firstMipData.size());
	}

	if (m_thumbnailGenerationRequested)
	{
		std::string iconPath = computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);

		if (generateThumbnail(fullFilePath, iconPath))
		{
			m_updateAssetInUICallback(computeName(), iconPath.substr(3, iconPath.size()), m_assetId);
		}

		if (m_dataOnCPUStatus != DataOnCPUStatus::AVAILABLE)
		{
			deleteImageData();
		}
	}
}
