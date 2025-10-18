#include "ResourceManager.h"

#include <glm/gtc/packing.hpp>
#include <fstream>
#include <stb_image_write.h>

#include <ImageFileLoader.h>
#include <MipMapGenerator.h>

#include <ProfilerCommon.h>
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
	PROFILE_FUNCTION

	for (Wolf::ResourceUniqueOwner<Mesh>& mesh : m_meshes)
	{
		mesh->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	for (Wolf::ResourceUniqueOwner<Image>& image : m_images)
	{
		image->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	if (m_currentResourceNeedRebuildFlags != 0)
	{
		if (m_currentResourceNeedRebuildFlags & static_cast<uint32_t>(MeshResourceEditor::ResourceEditorNotificationFlagBits::MESH))
		{
			m_meshes[m_currentResourceInEdition]->forceReload(m_materialsGPUManager, m_thumbnailsGenerationPass);
		}
		if (m_currentResourceNeedRebuildFlags & static_cast<uint32_t>(MeshResourceEditor::ResourceEditorNotificationFlagBits::PHYSICS))
		{
			Wolf::ModelData* modelData = m_meshes[m_currentResourceInEdition]->getModelData();
			modelData->physicsShapes.clear();

			for (uint32_t i = 0; i < m_meshResourceEditor->getMeshCount(); ++i)
			{
				Wolf::Physics::PhysicsShapeType shapeType = m_meshResourceEditor->getShapeType(i);

				if (shapeType == Wolf::Physics::PhysicsShapeType::Rectangle)
				{
					glm::vec3 p0;
					glm::vec3 s1;
					glm::vec3 s2;
					m_meshResourceEditor->computeInfoForRectangle(i, p0, s1, s2);

					modelData->physicsShapes.emplace_back(new Wolf::Physics::Rectangle(m_meshResourceEditor->getShapeName(i), p0, s1, s2));
				}
			}
		}
		m_meshes[m_currentResourceInEdition]->onChanged();

		m_currentResourceNeedRebuildFlags = 0;
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
	m_images.clear();
}

Wolf::ResourceNonOwner<Entity> ResourceManager::computeResourceEditor(ResourceId resourceId)
{
	if (m_currentResourceInEdition == resourceId)
		return m_transientEditionEntity.createNonOwnerResource();

	m_transientEditionEntity.reset(new Entity("", [](Entity*) {}));
	m_transientEditionEntity->setIncludeEntityParams(false);

	if (isMesh(resourceId))
	{
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

		if (!meshResourceEditor)
		{
			Wolf::ModelData* modelData = getModelData(resourceId);
			meshResourceEditor = new MeshResourceEditor(m_meshes[resourceId - MESH_RESOURCE_IDX_OFFSET]->getLoadingPath(), m_requestReloadCallback, modelData->isMeshCentered);

			for (Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& shape : modelData->physicsShapes)
			{
				meshResourceEditor->addShape(shape);
			}

			meshResourceEditor->subscribe(this, [this](Notifier::Flags flags) { onResourceEditionChanged(flags); });
		}
		m_transientEditionEntity->addComponent(meshResourceEditor);
		m_meshResourceEditor = m_transientEditionEntity->getComponent<MeshResourceEditor>();
	}
	else if (isImage(resourceId))
	{

	}
	else
	{
		Wolf::Debug::sendError("Resource type is not supported");
	}

	m_currentResourceInEdition = resourceId;

	return m_transientEditionEntity.createNonOwnerResource();
}

bool ResourceManager::isMesh(ResourceId resourceId)
{
	return resourceId >= MESH_RESOURCE_IDX_OFFSET && resourceId < MESH_RESOURCE_IDX_OFFSET + MAX_MESH_RESOURCE_COUNT;
}

bool ResourceManager::isImage(ResourceId resourceId)
{
	return resourceId >= IMAGE_RESOURCE_IDX_OFFSET && resourceId < IMAGE_RESOURCE_IDX_OFFSET + MAX_IMAGE_RESOURCE_COUNT;
}

ResourceManager::ResourceId ResourceManager::addModel(const std::string& loadingPath)
{
	if (m_meshes.size() >= MAX_MESH_RESOURCE_COUNT)
	{
		Wolf::Debug::sendError("Maximum mesh resources reached");
		return NO_RESOURCE;
	}

	for (uint32_t i = 0; i < m_meshes.size(); ++i)
	{
		if (m_meshes[i]->getLoadingPath() == loadingPath)
		{
			return i + MESH_RESOURCE_IDX_OFFSET;
		}
	}

	std::string iconFullPath = computeIconPath(loadingPath);
	bool iconFileExists = formatIconPath(loadingPath, iconFullPath);

	ResourceId resourceId = static_cast<uint32_t>(m_meshes.size()) + MESH_RESOURCE_IDX_OFFSET;

	m_meshes.emplace_back(new Mesh(loadingPath, !iconFileExists, resourceId, m_updateResourceInUICallback));
	m_addResourceToUICallback(m_meshes.back()->computeName(), iconFullPath, resourceId);

	return resourceId;
}

bool ResourceManager::isModelLoaded(ResourceId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return modelResourceId != NO_RESOURCE && m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->isLoaded();
}

Wolf::ModelData* ResourceManager::getModelData(ResourceId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->getModelData();
}

Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure> ResourceManager::getBLAS(ResourceId modelResourceId)
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->getBLAS();
}

uint32_t ResourceManager::getFirstMaterialIdx(ResourceId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->getFirstMaterialIdx();
}

uint32_t ResourceManager::getFirstTextureSetIdx(ResourceId modelResourceId) const
{
	if (!isMesh(modelResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[modelResourceId - MESH_RESOURCE_IDX_OFFSET]->getFirstTextureSetIdx();
}

void ResourceManager::subscribeToResource(ResourceId resourceId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const
{
	if (!isMesh(resourceId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	m_meshes[resourceId - MESH_RESOURCE_IDX_OFFSET]->subscribe(instance, callback);
}

ResourceManager::ResourceId ResourceManager::addImage(const std::string& loadingPath, bool loadMips, bool isSRGB, bool isHDR, bool keepDataOnCPU)
{
	if (m_images.size() >= MAX_IMAGE_RESOURCE_COUNT)
	{
		Wolf::Debug::sendError("Maximum image resources reached");
		return NO_RESOURCE;
	}

	for (uint32_t i = 0; i < m_images.size(); ++i)
	{
		if (m_images[i]->getLoadingPath() == loadingPath)
		{
			return i + IMAGE_RESOURCE_IDX_OFFSET;
		}
	}

	ResourceId resourceId = static_cast<uint32_t>(m_images.size()) + IMAGE_RESOURCE_IDX_OFFSET;

	std::string iconFullPath = computeIconPath(loadingPath);
	bool iconFileExists = formatIconPath(loadingPath, iconFullPath);

	m_images.emplace_back(new Image(loadingPath, !iconFileExists, resourceId, m_updateResourceInUICallback, loadMips, isSRGB, isHDR, keepDataOnCPU));
	m_addResourceToUICallback(m_images.back()->computeName(), iconFullPath, resourceId);

	return resourceId;
}

bool ResourceManager::isImageLoaded(ResourceId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}

	return m_images[imageResourceId - IMAGE_RESOURCE_IDX_OFFSET]->isLoaded();
}

Wolf::ResourceNonOwner<Wolf::Image> ResourceManager::getImage(ResourceId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}

	return m_images[imageResourceId - IMAGE_RESOURCE_IDX_OFFSET]->getImage();
}

const uint8_t* ResourceManager::getImageData(ResourceId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}

	return m_images[imageResourceId - IMAGE_RESOURCE_IDX_OFFSET]->getFirstMipData();
}

void ResourceManager::deleteImageData(ResourceId imageResourceId) const
{
	if (!isImage(imageResourceId))
	{
		Wolf::Debug::sendError("ResourceId is not an image");
	}
	m_images[imageResourceId - IMAGE_RESOURCE_IDX_OFFSET]->deleteImageData();
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
	std::string extension = loadingPath.substr(loadingPath.find_last_of('.') + 1);
	std::string thumbnailFormat = extension == "dae" || extension == "cube" ? ".gif" : ".png";

	return "UI/media/resourceIcon/" + computeModelFullIdentifier(loadingPath) + thumbnailFormat;
}

bool ResourceManager::formatIconPath(const std::string& inLoadingPath, std::string& outIconPath)
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
	if (isImage(m_currentResourceInEdition))
	{
		return; // nothing to save for images
	}

	if (m_currentResourceInEdition != NO_RESOURCE)
	{
		m_resourceEditedParamsToSaveCount++;
		ResourceEditedParamsToSave& resourceEditedParamsToSave = m_resourceEditedParamsToSave[m_resourceEditedParamsToSaveCount - 1];
		resourceEditedParamsToSave.resourceId = m_currentResourceInEdition;
		resourceEditedParamsToSave.meshResourceEditor.reset(m_transientEditionEntity->releaseComponent<MeshResourceEditor>());

		resourceEditedParamsToSave.meshResourceEditor->unregisterEntity();
	}
}

void ResourceManager::onResourceEditionChanged(Notifier::Flags flags)
{
	m_currentResourceNeedRebuildFlags |= flags;
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

	if (m_meshToKeepInMemory)
	{
		m_meshToKeepInMemory.reset(nullptr);
	}
}

void ResourceManager::Mesh::forceReload(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
	const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	m_meshToKeepInMemory.reset(m_modelData.mesh.release());

	loadModel(materialsGPUManager, thumbnailsGenerationPass);
	m_modelLoadingRequested = false;
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
	if (g_editorConfiguration->getEnableRayTracing())
	{
		VkBufferUsageFlags rayTracingFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		modelLoadingInfo.additionalVertexBufferUsages |= rayTracingFlags;
		modelLoadingInfo.additionalIndexBufferUsages |= rayTracingFlags;
	}
	Wolf::ModelLoader::loadObject(m_modelData, modelLoadingInfo);

	materialsGPUManager->lockMaterials();
	materialsGPUManager->lockTextureSets();

	m_firstTextureSetIdx = m_modelData.textureSets.empty() ? 0 : materialsGPUManager->getCurrentTextureSetCount();
	m_firstMaterialIdx = m_modelData.textureSets.empty() ? 0 : materialsGPUManager->getCurrentMaterialCount();
	for (const Wolf::MaterialsGPUManager::TextureSetInfo& textureSetInfo : m_modelData.textureSets)
	{
		materialsGPUManager->addNewTextureSet(textureSetInfo);

		Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
		materialInfo.textureSetInfos[0].textureSetIdx = materialsGPUManager->getTextureSetsCacheInfo().back().textureSetIdx;
		materialsGPUManager->addNewMaterial(materialInfo);
	}

	materialsGPUManager->unlockTextureSets();
	materialsGPUManager->unlockMaterials();

	if (m_thumbnailGenerationRequested)
	{
		Wolf::AABB entityAABB = m_modelData.mesh->getAABB();
		float entityHeight = entityAABB.getMax().y - entityAABB.getMin().y;

		glm::vec3 position = entityAABB.getCenter() + glm::vec3(-entityHeight, entityHeight, -entityHeight);
		glm::vec3 target = entityAABB.getCenter();
		glm::mat4 viewMatrix = glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));

		thumbnailsGenerationPass->addRequestBeforeFrame({ &m_modelData, computeIconPath(m_loadingPath), 
			[this]() { m_updateResourceInUICallback(computeName(), computeIconPath(m_loadingPath).substr(3, computeIconPath(m_loadingPath).size()), m_resourceId); }, viewMatrix });
		m_thumbnailGenerationRequested = false;
	}

	if (g_editorConfiguration->getEnableRayTracing())
	{
		buildBLAS();
	}
}

void ResourceManager::Mesh::buildBLAS()
{
	Wolf::GeometryInfo geometryInfo;
	geometryInfo.mesh.vertexBuffer = &*m_modelData.mesh->getVertexBuffer();
	geometryInfo.mesh.vertexCount = m_modelData.mesh->getVertexCount();
	geometryInfo.mesh.vertexSize = m_modelData.mesh->getVertexSize();
	geometryInfo.mesh.vertexFormat = Wolf::Format::R32G32B32_SFLOAT;
	geometryInfo.mesh.indexBuffer = &*m_modelData.mesh->getIndexBuffer();
	geometryInfo.mesh.indexCount = m_modelData.mesh->getIndexCount();

	Wolf::BottomLevelAccelerationStructureCreateInfo createInfo{};
	createInfo.geometryInfos = { &geometryInfo, 1 };
	createInfo.buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

	m_bottomLevelAccelerationStructure.reset(Wolf::BottomLevelAccelerationStructure::createBottomLevelAccelerationStructure(createInfo));
}

ResourceManager::Image::Image(const std::string& loadingPath, bool needThumbnailsGeneration, ResourceId resourceId, const std::function<void(const std::string&, const std::string&, ResourceId)>& updateResourceInUICallback,
	bool loadMips, bool isSRGB, bool isHDR, bool keepDataOnCPU)
	: ResourceInterface(loadingPath, resourceId, updateResourceInUICallback), m_thumbnailGenerationRequested(needThumbnailsGeneration), m_loadMips(loadMips), m_isSRGB(isSRGB), m_isHDR(isHDR)
{
	if (m_isSRGB && m_isHDR)
	{
		Wolf::Debug::sendError("Can't use sRGB with HDR");
	}
	if (m_loadMips && m_isHDR)
	{
		Wolf::Debug::sendError("Mips are not supported with HDR");
	}

	m_imageLoadingRequested = true;
	m_dataOnCPUStatus = keepDataOnCPU ? DataOnCPUStatus::NOT_LOADED_YET : DataOnCPUStatus::NEVER_KEPT;
}

void ResourceManager::Image::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	if (m_imageLoadingRequested)
	{
		loadImage();
		m_imageLoadingRequested = false;
	}
}

bool ResourceManager::Image::isLoaded() const
{
	return static_cast<bool>(m_image);
}

const uint8_t* ResourceManager::Image::getFirstMipData() const
{
	if (m_dataOnCPUStatus != DataOnCPUStatus::AVAILABLE)
	{
		Wolf::Debug::sendError("Data requested in not here");
		return nullptr;
	}

	return m_firstMipData.data();
}

void ResourceManager::Image::deleteImageData()
{
	m_firstMipData.clear();
	m_dataOnCPUStatus = DataOnCPUStatus::DELETED;
}

void ResourceManager::Image::loadImage()
{
	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(m_loadingPath);
	Wolf::ImageFileLoader imageFileLoader(fullFilePath, m_isHDR);

	Wolf::CreateImageInfo createImageInfo;
	createImageInfo.extent = { imageFileLoader.getWidth(), imageFileLoader.getHeight(), imageFileLoader.getDepth() };
	createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	Wolf::Format imageFormat = imageFileLoader.getFormat();
	if (m_isSRGB)
	{
		if (imageFormat == Wolf::Format::R8G8B8A8_UNORM)
		{
			imageFormat = Wolf::Format::R8G8B8A8_SRGB;
		}
		else
		{
			Wolf::Debug::sendError("Format is not suppported for sRGB");
		}
	}

	Wolf::ResourceUniqueOwner<Wolf::MipMapGenerator> mipMapGenerator;
	uint32_t mipLevelCount = 1;
	if (m_loadMips)
	{
		if (imageFileLoader.getDepth())
		{
			Wolf::Debug::sendError("Load mip with 3D images is not supported yet");
		}

		mipMapGenerator.reset(new Wolf::MipMapGenerator(imageFileLoader.getPixels(), { imageFileLoader.getWidth(), imageFileLoader.getHeight() }, imageFormat));
		mipLevelCount = mipMapGenerator->getMipLevelCount();
	}

	createImageInfo.format = imageFormat;
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

			if (!gifEncoder.open(computeIconPath(m_loadingPath), ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE, quality, useGlobalColorMap, loop, preAllocSize))
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
				stbi_write_png(computeIconPath(m_loadingPath).c_str(), ThumbnailsGenerationPass::OUTPUT_SIZE, ThumbnailsGenerationPass::OUTPUT_SIZE,
					4, thumbnailPixels.data(), ThumbnailsGenerationPass::OUTPUT_SIZE * sizeof(Wolf::ImageCompression::RGBA8));
			}
			else
			{
				hadAnErrorDuringGeneration = true;
			}
		}

		if (!hadAnErrorDuringGeneration)
		{
			m_updateResourceInUICallback(computeName(), computeIconPath(m_loadingPath).substr(3, computeIconPath(m_loadingPath).size()), m_resourceId);
		}
	}
}
