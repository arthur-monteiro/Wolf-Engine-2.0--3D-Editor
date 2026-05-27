#include "AssetManager.h"

#include <glm/gtc/packing.hpp>
#include <fstream>

#include <sol/sol.hpp>

#include <ImageFileLoader.h>
#include <MipMapGenerator.h>

#include <ProfilerCommon.h>

#include "BinaryWriter.h"
#include "EditorConfiguration.h"
#include "Entity.h"
#include "ImageFormatter.h"
#include "MeshAssetEditor.h"
#include "ExternalSceneLoader.h"
#include "TextureSetEditor.h"

AssetManager* AssetManager::ms_assetManager;

AssetManager::AssetManager(const std::function<void(const std::string&)>& evaluateJSScript, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
		const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU, const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface)
	: m_evaluateJSScript(evaluateJSScript), m_editorConfiguration(editorConfiguration), m_materialsGPUManager(materialsGPUManager), m_thumbnailsGenerationPass(renderingPipeline->getThumbnailsGenerationPass()),
	  m_isolateMeshCallback(isolateMeshCallback), m_removeIsolationAndGetViewMatrixCallback(removeIsolationAndGetViewMatrixCallback), m_renderingPipeline(renderingPipeline), m_editorPushDataToGPU(editorPushDataToGPU),
	  m_bufferPoolInterface(bufferPoolInterface)
{
	ms_assetManager = this;
	m_onAssetUpdatedCallback = [this](AssetId assetId) { onAssetUpdated(assetId); };
}

void AssetManager::updateBeforeFrame()
{
	PROFILE_FUNCTION

	for (uint32_t i = 0; i < m_externalScenes.size(); ++i)
	{
		Wolf::ResourceUniqueOwner<AssetExternalScene>& externalScene = m_externalScenes[i];
		externalScene->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	for (uint32_t i = 0; i < m_meshes.size(); ++i)
	{
		Wolf::ResourceUniqueOwner<AssetMesh>& mesh = m_meshes[i];
		mesh->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	for (uint32_t i = 0; i < m_images.size(); ++i)
	{
		Wolf::ResourceUniqueOwner<AssetImage>& image = m_images[i];
		image->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	for (uint32_t i = 0; i < m_textureSets.size(); ++i)
	{
		Wolf::ResourceUniqueOwner<AssetTextureSet>& assetTextureSet = m_textureSets[i];
		assetTextureSet->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	for (uint32_t i = 0; i < m_materials.size(); ++i)
	{
		Wolf::ResourceUniqueOwner<AssetMaterial>& assetMaterial = m_materials[i];
		assetMaterial->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}

	for (uint32_t i = 0; i < m_particles.size(); ++i)
	{
		Wolf::ResourceUniqueOwner<AssetParticle>& assetParticle = m_particles[i];
		assetParticle->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
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

			// for (uint32_t i = 0; i < m_meshAssetEditor->getPhysicsMeshCount(); ++i)
			// {
			// 	Wolf::Physics::PhysicsShapeType shapeType = m_meshAssetEditor->getShapeType(i);
			//
			// 	if (shapeType == Wolf::Physics::PhysicsShapeType::Rectangle)
			// 	{
			// 		glm::vec3 p0;
			// 		glm::vec3 s1;
			// 		glm::vec3 s2;
			// 		m_meshAssetEditor->computeInfoForRectangle(i, p0, s1, s2);
			//
			// 		physicsShapes.emplace_back(new Wolf::Physics::Rectangle(m_meshAssetEditor->getShapeName(i), p0, s1, s2));
			// 	}
			// 	else if (shapeType == Wolf::Physics::PhysicsShapeType::Box)
			// 	{
			// 		glm::vec3 aabbMin;
			// 		glm::vec3 aabbMax;
			// 		glm::vec3 rotation;
			// 		m_meshAssetEditor->computeInfoForBox(i, aabbMin, aabbMax, rotation);
			//
			// 		physicsShapes.emplace_back(new Wolf::Physics::Box(m_meshAssetEditor->getShapeName(i), aabbMin, aabbMax, rotation));
			// 	}
			// }
		}
		m_meshes[m_currentAssetInEdition]->onChanged();

		m_currentAssetNeedRebuildFlags = 0;
	}

	// UI update
	if (m_assetAdded)
	{
		m_evaluateJSScript("AssetBrowser.resetAndRequest()");
		m_assetAdded = false;
	}
}

void AssetManager::save(std::ofstream& outFile) const
{
	outFile << "\t\"assets\": {\n";

	auto addAssets = [&outFile](const auto& assets, const std::string& name)
	{
		std::vector<std::string> results;

		for (uint32_t i = 0; i < assets.size(); ++i)
		{
			const auto& asset = assets[i];

			std::stringstream stringStream;
			if (saveAsset(stringStream, asset.template createNonOwnerResource<AssetInterface>()))
			{
				results.push_back(stringStream.str());
			}
		}

		outFile << "\t\t\"" + name + "\": [\n";
		for (size_t i = 0; i < results.size(); ++i)
		{
			outFile << results[i];
			if (i < results.size() - 1) {
				outFile << ",";
			}
			outFile << "\n";
		}
		outFile << "\t\t],\n";
	};

	addAssets(m_images, "images");
	addAssets(m_externalScenes, "externalScenes");
	addAssets(m_textureSets, "textureSets");
	addAssets(m_materials, "materials");
	addAssets(m_particles, "particles");

	outFile << "\t},\n";
}

void AssetManager::clear()
{
	releaseAllEditorsFromTransientEntity();

	m_meshes.clear();
	m_images.clear();
	m_combinedImages.clear();
}

void AssetManager::releaseRenderingPipeline()
{
	m_thumbnailsGenerationPass.release();
	m_renderingPipeline.release();
}

Wolf::ResourceNonOwner<Entity> AssetManager::computeAssetEditor(AssetId assetId)
{
	if (m_currentAssetInEdition == assetId)
		return m_transientEditionEntity.createNonOwnerResource();

	releaseAllEditorsFromTransientEntity();

	m_transientEditionEntity.reset(new Entity("", [](Entity*) {}, [](Entity*) {},
		[](const std::string&) { return Wolf::NullableResourceNonOwner<Entity>(); }));
	m_transientEditionEntity->setIncludeEntityParams(false);

	Wolf::NullableResourceNonOwner<AssetInterface> assetInterface;

	if (isMesh(assetId))
	{
		assetInterface = m_meshes[assetId - MESH_ASSET_IDX_OFFSET].createNonOwnerResource<AssetInterface>();
	}
	else if (isImage(assetId))
	{
		assetInterface = m_images[assetId - IMAGE_ASSET_IDX_OFFSET].createNonOwnerResource<AssetInterface>();
	}
	else if (isCombinedImage(assetId))
	{
		assetInterface = m_combinedImages[assetId - COMBINED_IMAGE_ASSET_IDX_OFFSET].createNonOwnerResource<AssetInterface>();
	}
	else if (isTextureSet(assetId))
	{
		assetInterface = m_textureSets[assetId - TEXTURE_SET_ASSET_IDX_OFFSET].createNonOwnerResource<AssetInterface>();
	}
	else if (isExternalScene(assetId))
	{
		assetInterface = m_externalScenes[assetId - EXTERNAL_SCENE_ASSET_IDX_OFFSET].createNonOwnerResource<AssetInterface>();
	}
	else if (isMaterial(assetId))
	{
		assetInterface = m_materials[assetId - MATERIAL_ASSET_IDX_OFFSET].createNonOwnerResource<AssetInterface>();
	}
	else if (isParticle(assetId))
	{
		assetInterface = m_particles[assetId - PARTICLE_ASSET_IDX_OFFSET].createNonOwnerResource<AssetInterface>();
	}
	else
	{
		Wolf::Debug::sendCriticalError("Asset type is not supported");
	}

	std::vector<Wolf::ResourceNonOwner<ComponentInterface>> editors;
	assetInterface->getEditors(editors);
	for (Wolf::ResourceNonOwner<ComponentInterface>& editor : editors)
	{
		m_transientEditionEntity->addComponent(&*editor);
	}

	m_currentAssetInEdition = assetId;

	return m_transientEditionEntity.createNonOwnerResource();
}

void AssetManager::bindUltralightCallbacks(ultralight::JSObject& jsObject)
{
	jsObject["requestAssetPayload"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&AssetManager::requestAssetPayloadJSCallback, this, std::placeholders::_1, std::placeholders::_2));
}

void AssetManager::addScriptCallbacks(sol::state& state)
{
	// Mesh
	state.set_function("getMeshFromAssetId", [this](AssetId assetId) -> AssetMesh*
	{
		if (!isMesh(assetId))
		{
			Wolf::Debug::sendCriticalError("Asset type is a mesh");
		}
		size_t index = assetId - MESH_ASSET_IDX_OFFSET;

		if (index < m_meshes.size())
		{
			return &*m_meshes[index];
		}

		Wolf::Debug::sendCriticalError("Index out of range");
		return nullptr;
	});

	state.new_usertype<AssetMesh>("Mesh",
		"getLoadingPath", [](const AssetMesh& self) { return self.getLoadingPath(); },
		"computeMeshFormatter", [](AssetMesh& self) { return self.computeMeshFormatter(); }
	);
	state.set_function("deleteMeshFormatter", [](const MeshFormatter* meshFormatter) { delete meshFormatter; });

	state.new_usertype<MeshFormatter>("MeshFormatter",
		"getAABB", [](const MeshFormatter& self) { return self.getAABB(); },
		"getLODCount", [](MeshFormatter& self) { return self.getDefaultLODInfo().size() + 1; },
		"getVertices", [](MeshFormatter& self, uint32_t lod) -> const std::vector<Vertex3D>&
		{
			if (lod == 0)
				return self.getStaticVertices();
			else
				return self.getDefaultLODInfo()[lod - 1].m_staticVertices;
		},
		"getIndices", [](MeshFormatter& self, uint32_t lod) -> const std::vector<uint32_t>&
		{
			if (lod == 0)
				return self.getIndices();
			else
				return self.getDefaultLODInfo()[lod - 1].m_indices;
		}
	);

	state.new_usertype<Vertex3D>("Vertex3D",
		"position", &Vertex3D::pos,
		"normal", &Vertex3D::normal,
		"texCoords", &Vertex3D::texCoord
	);

	// Material
	state.set_function("getMaterialFromAssetId", [this](AssetId assetId) -> AssetMaterial*
	{
		if (!isMaterial(assetId))
		{
			Wolf::Debug::sendCriticalError("Asset type is a material");
		}
		size_t index = assetId - MATERIAL_ASSET_IDX_OFFSET;

		if (index < m_materials.size())
		{
			return &*m_materials[index];
		}

		Wolf::Debug::sendCriticalError("Index out of range");
		return nullptr;
	});

	state.new_usertype<AssetMaterial>("Material",
		"getLoadingPath", [](const AssetMaterial& self) { return self.getLoadingPath(); },
		"getTextureSetCount", [](const AssetMaterial& self) { return self.getEditor()->getTextureSetCount(); },
		"getTextureSetAssetId", [](const AssetMaterial& self, uint32_t idx) { return self.getEditor()->getTextureSetAssetId(idx); }
	);

	// Texture set
	state.set_function("getTextureSetFromAssetId", [this](AssetId assetId) -> AssetTextureSet*
	{
		if (!isTextureSet(assetId))
		{
			Wolf::Debug::sendCriticalError("Asset type is a texture set");
		}
		size_t index = assetId - TEXTURE_SET_ASSET_IDX_OFFSET;

		if (index < m_textureSets.size())
		{
			return &*m_textureSets[index];
		}

		Wolf::Debug::sendCriticalError("Index out of range");
		return nullptr;
	});

	state.new_usertype<AssetTextureSet>("TextureSet",
		"getLoadingPath", [](const AssetTextureSet& self) { return self.getLoadingPath(); },
		"getAlbedoAssetId", [](const AssetTextureSet& self) { return self.getTextureSetEditor()->getAlbedoAssetId(); }
	);

	// Image
	state.set_function("getImageFromAssetId", [this](AssetId assetId) -> AssetImage*
	{
		if (!isImage(assetId))
		{
			Wolf::Debug::sendCriticalError("Asset type is an image");
		}
		size_t index = assetId - IMAGE_ASSET_IDX_OFFSET;

		if (index < m_images.size())
		{
			return &*m_images[index];
		}

		Wolf::Debug::sendCriticalError("Index out of range");
		return nullptr;
	});

	state.new_usertype<AssetImage>("Image",
		"getLoadingPath", [](const AssetImage& self) { return self.getLoadingPath(); },
		"loadImageAndDump", [this](AssetImage& self, BinaryWriter* binaryWriter, uint32_t mipSkippedRequestedCount)
		{
			AssetImage::LoadingRequest request{};
			request.m_format = Wolf::Format::BC1_RGB_SRGB_BLOCK;
			request.m_keepDataMode = AssetImageInterface::KeepDataMode::ONLY_CPU;
			request.m_loadMips = true;
			request.m_canBeVirtualized = false;
			self.requestImageLoading(request);
			self.updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);

			Wolf::ResourceNonOwner<Wolf::Image> image = self.getImage(request.m_format);

			binaryWriter->writeUInt(static_cast<uint32_t>(request.m_format));

			uint32_t mipLevelCount = image->getMipLevelCount();
			uint32_t mipSkipped = 0;
			if (mipLevelCount > mipSkippedRequestedCount + 1)
			{
				mipSkipped = mipSkippedRequestedCount;
				mipLevelCount -= mipSkippedRequestedCount;
			}
			else
			{
				mipSkipped = mipLevelCount - 1;
				mipLevelCount = 1;
			}

			const Wolf::Extent3D extent = image->getExtent();
			binaryWriter->writeUInt(extent.width >> mipSkipped);
			binaryWriter->writeUInt(extent.height >> mipSkipped);
			binaryWriter->writeUInt(extent.depth);

			binaryWriter->writeUInt(mipLevelCount);

			for (uint32_t mipLevel = mipSkipped; mipLevel < image->getMipLevelCount(); ++mipLevel)
			{
				uint32_t mipPixelCount = (extent.width >> mipLevel) * (extent.height >> mipLevel) * extent.depth;
				binaryWriter->writeUInt(mipPixelCount);

				uint32_t copySize = mipPixelCount * Wolf::Image::computeBPPFromFormat(request.m_format);
				binaryWriter->writeData(self.getMipData(mipLevel, request.m_format), copySize);
			}

			self.deleteImageData(request.m_format);
		}
	);
}

AssetId AssetManager::getAssetIdForPath(const std::string& path)
{
	for (uint32_t meshIdx = 0; meshIdx < m_meshes.size(); ++meshIdx)
	{
		if (m_meshes[meshIdx]->getLoadingPath() == path)
		{
			return meshIdx + MESH_ASSET_IDX_OFFSET;
		}
	}

	for (uint32_t imageIdx = 0; imageIdx < m_images.size(); ++imageIdx)
	{
		if (m_images[imageIdx]->getLoadingPath() == path)
		{
			return imageIdx + IMAGE_ASSET_IDX_OFFSET;
		}
	}

	for (uint32_t externalSceneIdx = 0; externalSceneIdx < m_externalScenes.size(); ++externalSceneIdx)
	{
		if (m_externalScenes[externalSceneIdx]->getLoadingPath() == path)
		{
			return externalSceneIdx + EXTERNAL_SCENE_ASSET_IDX_OFFSET;
		}
	}

	for (uint32_t textureSetIdx = 0; textureSetIdx < m_textureSets.size(); ++textureSetIdx)
	{
		if (m_textureSets[textureSetIdx]->getLoadingPath() == path)
		{
			return textureSetIdx + TEXTURE_SET_ASSET_IDX_OFFSET;
		}
	}

	for (uint32_t materialIdx = 0; materialIdx < m_materials.size(); ++materialIdx)
	{
		if (m_materials[materialIdx]->getLoadingPath() == path)
		{
			return materialIdx + MATERIAL_ASSET_IDX_OFFSET;
		}
	}

	for (uint32_t particleIdx = 0; particleIdx < m_particles.size(); ++particleIdx)
	{
		if (m_particles[particleIdx]->getLoadingPath() == path)
		{
			return particleIdx + PARTICLE_ASSET_IDX_OFFSET;
		}
	}

	Wolf::Debug::sendCriticalError("Path not found");
	return NO_ASSET;
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

bool AssetManager::isScene(AssetId assetId)
{
	return assetId == NO_ASSET || (assetId >= EXTERNAL_SCENE_ASSET_IDX_OFFSET && assetId < EXTERNAL_SCENE_ASSET_IDX_OFFSET + MAX_EXTERNAL_SCENE_ASSET_COUNT);
}

bool AssetManager::isTextureSet(AssetId assetId)
{
	return assetId == NO_ASSET || (assetId >= TEXTURE_SET_ASSET_IDX_OFFSET && assetId < TEXTURE_SET_ASSET_IDX_OFFSET + MAX_TEXTURE_SET_ASSET_COUNT);
}

bool AssetManager::isExternalScene(AssetId assetId)
{
	return assetId == NO_ASSET || (assetId >= EXTERNAL_SCENE_ASSET_IDX_OFFSET && assetId < EXTERNAL_SCENE_ASSET_IDX_OFFSET + MAX_EXTERNAL_SCENE_ASSET_COUNT);
}

bool AssetManager::isMaterial(AssetId assetId)
{
	return assetId == NO_ASSET || (assetId >= MATERIAL_ASSET_IDX_OFFSET && assetId < MATERIAL_ASSET_IDX_OFFSET + MAX_MATERIAL_ASSET_COUNT);
}

bool AssetManager::isParticle(AssetId assetId)
{
	return assetId == NO_ASSET || (assetId >= PARTICLE_ASSET_IDX_OFFSET && assetId < PARTICLE_ASSET_IDX_OFFSET + MAX_PARTICLE_ASSET_COUNT);
}

bool AssetManager::isMeshLoaded(AssetId assetId) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return assetId != NO_ASSET && m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->isLoaded();
}

Wolf::ResourceNonOwner<Wolf::Mesh> AssetManager::getMesh(AssetId assetId) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getMesh();
}

bool AssetManager::isMeshAnimated(AssetId assetId) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->isAnimated();
}

const std::vector<MeshFormatter::LODInfo>& AssetManager::getMeshDefaultLODInfo(AssetId assetId) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getDefaultLODInfo();
}

const std::vector<MeshFormatter::LODInfo>& AssetManager::getMeshSloppyLODInfo(AssetId assetId) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getSloppyLODInfo();
}

std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> AssetManager::getMeshDefaultSimplifiedMeshes(AssetId assetId) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getDefaultSimplifiedMeshes();
}

std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> AssetManager::getMeshSloppySimplifiedMeshes(AssetId assetId) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("ResourceId is not a mesh");
	}

	return m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getSloppySimplifiedMeshes();
}

Wolf::ResourceNonOwner<AnimationData> AssetManager::getAnimationData(AssetId assetId) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getAnimationData();
}

Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> AssetManager::getBLAS(AssetId assetId, uint32_t lod, uint32_t lodType)
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->getBLAS(lod, lodType);
}

std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>>& AssetManager::getPhysicsShapes(AssetId modelAssetId) const
{
	if (!isMesh(modelAssetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return m_meshes[modelAssetId - MESH_ASSET_IDX_OFFSET]->getPhysicsShapes();
}

uint32_t AssetManager::getDefaultMeshMaterialAssetId(AssetId meshAssetId) const
{
	if (!isMesh(meshAssetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a mesh");
	}

	return m_meshes[meshAssetId - MESH_ASSET_IDX_OFFSET]->getDefaultMaterialAssetId();
}

std::string AssetManager::computeModelName(AssetId modelAssetId) const
{
	if (!isMesh(modelAssetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	return m_meshes[modelAssetId - MESH_ASSET_IDX_OFFSET]->computeName();
}

void AssetManager::subscribeToMesh(AssetId assetId, const void* instance, const std::function<void(Notifier::Flags)>& callback) const
{
	if (!isMesh(assetId))
	{
		Wolf::Debug::sendError("AssetId is not a mesh");
	}

	m_meshes[assetId - MESH_ASSET_IDX_OFFSET]->subscribe(instance, callback);
}

AssetId AssetManager::addImage(const std::string& loadingPath, AssetId parentAssetId)
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
		bool iconFileExists = formatIconPath(iconFullPath);

		m_images.emplace_back(new AssetImage(m_editorPushDataToGPU, loadingPath, !iconFileExists, assetId, m_onAssetUpdatedCallback, parentAssetId));
		m_assetAdded = true;
	}

	return assetId;
}

Wolf::ResourceNonOwner<AssetImage> AssetManager::getAssetImage(AssetId assetId) const
{
	if (!isImage(assetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[assetId - IMAGE_ASSET_IDX_OFFSET].createNonOwnerResource();
}

bool AssetManager::isImageLoaded(AssetId assetId) const
{
	if (!isImage(assetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[assetId - IMAGE_ASSET_IDX_OFFSET]->isLoaded();
}

void AssetManager::requestImageLoading(AssetId assetId, const AssetImageInterface::LoadingRequest& loadingRequest, bool requestImmediateLoading)
{
	if (!isImage(assetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	m_images[assetId - IMAGE_ASSET_IDX_OFFSET]->requestImageLoading(loadingRequest);
	if (requestImmediateLoading)
	{
		m_images[assetId - IMAGE_ASSET_IDX_OFFSET]->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}
}

Wolf::ResourceNonOwner<Wolf::Image> AssetManager::getImage(AssetId imageAssetId, Wolf::Format format) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->getImage(format);
}

const uint8_t* AssetManager::getImageData(AssetId imageAssetId, uint32_t mipLevel, Wolf::Format format) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->getMipData(mipLevel, format);
}

void AssetManager::deleteImageData(AssetId imageAssetId, Wolf::Format format) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->deleteImageData(format);
}

void AssetManager::releaseImage(AssetId imageAssetId) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}
	m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->releaseImages();
}

std::string AssetManager::getImageSlicesFolder(AssetId imageAssetId) const
{
	if (!isImage(imageAssetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[imageAssetId - IMAGE_ASSET_IDX_OFFSET]->getSlicesFolder();
}

std::string AssetManager::getImageLoadingPath(AssetId assetId) const
{
	if (!isImage(assetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[assetId - IMAGE_ASSET_IDX_OFFSET]->getLoadingPath();
}

Wolf::ResourceNonOwner<ImageEditor> AssetManager::getImageEditor(AssetId assetId) const
{
	if (!isImage(assetId))
	{
		Wolf::Debug::sendError("AssetId is not an image");
	}

	return m_images[assetId - IMAGE_ASSET_IDX_OFFSET]->getEditor();
}

AssetId AssetManager::addCombinedImage(const std::string& loadingPath, AssetId parentAssetId)
{
	if (m_combinedImages.size() >= MAX_COMBINED_IMAGE_ASSET_COUNT)
	{
		Wolf::Debug::sendCriticalError("Maximum combined image resources reached");
		return NO_ASSET;
	}

	AssetId assetId = NO_ASSET;

	for (uint32_t i = 0; i < m_combinedImages.size(); ++i)
	{
		if (m_combinedImages[i]->getLoadingPath() == loadingPath)
		{
			assetId = i + COMBINED_IMAGE_ASSET_IDX_OFFSET;
			break;
		}
	}

	if (assetId == NO_ASSET)
	{
		assetId = static_cast<uint32_t>(m_combinedImages.size()) + COMBINED_IMAGE_ASSET_IDX_OFFSET;

		std::string iconFullPath = computeIconPath(loadingPath, 0);
		bool iconFileExists = formatIconPath(iconFullPath);

		m_combinedImages.emplace_back(new AssetCombinedImage(m_editorPushDataToGPU, loadingPath, !iconFileExists, assetId, m_onAssetUpdatedCallback, this, parentAssetId));
		m_assetAdded = true;
	}

	return assetId;
}

void AssetManager::requestCombinedImageLoading(AssetId assetId, const AssetImageInterface::LoadingRequest& loadingRequest, bool requestImmediateLoading)
{
	if (!isCombinedImage(assetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a combined image");
	}

	m_combinedImages[assetId - COMBINED_IMAGE_ASSET_IDX_OFFSET]->requestImageLoading(loadingRequest);
	if (requestImmediateLoading)
	{
		m_combinedImages[assetId - COMBINED_IMAGE_ASSET_IDX_OFFSET]->updateBeforeFrame(m_materialsGPUManager, m_thumbnailsGenerationPass);
	}
}

Wolf::ResourceNonOwner<Wolf::Image> AssetManager::getCombinedImage(AssetId combinedImageAssetId, Wolf::Format format) const
{
	if (!isCombinedImage(combinedImageAssetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a combined image");
	}

	return m_combinedImages[combinedImageAssetId - COMBINED_IMAGE_ASSET_IDX_OFFSET]->getImage(format);
}

std::string AssetManager::getCombinedImageSlicesFolder(AssetId combinedImageAssetId) const
{
	if (!isCombinedImage(combinedImageAssetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a combined image");
	}

	return m_combinedImages[combinedImageAssetId - COMBINED_IMAGE_ASSET_IDX_OFFSET]->getSlicesFolder();
}

Wolf::ResourceNonOwner<CombinedImageEditor> AssetManager::getCombinedImageEditor(AssetId assetId) const
{
	if (!isCombinedImage(assetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a combined image");
	}

	return m_combinedImages[assetId - COMBINED_IMAGE_ASSET_IDX_OFFSET]->getEditor();
}

AssetId AssetManager::addExternalScene(const std::string& loadingPath)
{
	if (m_externalScenes.size() >= MAX_EXTERNAL_SCENE_ASSET_COUNT)
	{
		Wolf::Debug::sendCriticalError("Maximum external scene assets reached");
		return NO_ASSET;
	}

	for (uint32_t i = 0; i < m_externalScenes.size(); ++i)
	{
		if (m_externalScenes[i]->getLoadingPath() == loadingPath)
		{
			return i + EXTERNAL_SCENE_ASSET_IDX_OFFSET;
		}
	}

	std::string iconFullPath = computeIconPath(loadingPath, 0);
	bool iconFileExists = formatIconPath(iconFullPath);
	uint32_t thumbnailsCount = 0;
	while (iconFileExists)
	{
		thumbnailsCount++;

		std::string nextIconFullPath = computeIconPath(loadingPath, thumbnailsCount);
		iconFileExists = formatIconPath(nextIconFullPath);
	}

	iconFileExists = thumbnailsCount > 0;
	if (thumbnailsCount > 1) // We need to replace the original with the latest one
	{
		std::string originalPath = computeIconPath(loadingPath, 0);
		std::string latestPath = computeIconPath(loadingPath, thumbnailsCount - 1);

		std::filesystem::remove(originalPath);
		std::filesystem::rename(latestPath, originalPath);
	}

	AssetId assetId = static_cast<uint32_t>(m_externalScenes.size()) + EXTERNAL_SCENE_ASSET_IDX_OFFSET;

	AssetExternalScene* newExternalScene = new AssetExternalScene(this, loadingPath, !iconFileExists, assetId, m_onAssetUpdatedCallback, NO_ASSET);
	m_externalScenes.emplace_back(newExternalScene);
	m_assetAdded = true;

	return assetId;
}

bool AssetManager::isSceneLoaded(AssetId sceneAssetId) const
{
	if (!isScene(sceneAssetId))
	{
		Wolf::Debug::sendError("ResourceId is not a scene");
	}

	return sceneAssetId != NO_ASSET && m_externalScenes[sceneAssetId - EXTERNAL_SCENE_ASSET_IDX_OFFSET]->isLoaded();
}

Wolf::AABB AssetManager::getSceneAABB(AssetId sceneAssetId) const
{
	if (!isScene(sceneAssetId))
	{
		Wolf::Debug::sendError("ResourceId is not a scene");
		return Wolf::AABB();
	}

	if (sceneAssetId == NO_ASSET)
	{
		Wolf::Debug::sendError("Invalid asset ID");
		return Wolf::AABB();
	}

	return m_externalScenes[sceneAssetId - EXTERNAL_SCENE_ASSET_IDX_OFFSET]->getAABB();
}

const std::vector<AssetId>& AssetManager::getSceneModelAssetIds(AssetId sceneAssetId) const
{
	if (!isScene(sceneAssetId))
	{
		Wolf::Debug::sendCriticalError("ResourceId is not a scene");
	}

	if (sceneAssetId == NO_ASSET)
	{
		Wolf::Debug::sendCriticalError("Invalid asset ID");
	}

	return m_externalScenes[sceneAssetId - EXTERNAL_SCENE_ASSET_IDX_OFFSET]->getModelAssetIds();
}

const std::vector<ExternalSceneLoader::InstanceData>& AssetManager::getSceneInstances(AssetId sceneAssetId) const
{
	if (!isScene(sceneAssetId))
	{
		Wolf::Debug::sendCriticalError("ResourceId is not a scene");
	}

	if (sceneAssetId == NO_ASSET)
	{
		Wolf::Debug::sendCriticalError("Invalid asset ID");
	}

	return m_externalScenes[sceneAssetId - EXTERNAL_SCENE_ASSET_IDX_OFFSET]->getInstances();
}

AssetId AssetManager::addTextureSet(const std::string& loadingPath, AssetId parentAssetId)
{
	if (m_textureSets.size() >= MAX_TEXTURE_SET_ASSET_COUNT)
	{
		Wolf::Debug::sendCriticalError("Maximum texture set assets reached");
		return NO_ASSET;
	}

	AssetId assetId = NO_ASSET;

	for (uint32_t i = 0; i < m_textureSets.size(); ++i)
	{
		if (m_textureSets[i]->getLoadingPath() == loadingPath)
		{
			assetId = i + TEXTURE_SET_ASSET_IDX_OFFSET;
			break;
		}
	}

	if (assetId == NO_ASSET)
	{
		assetId = static_cast<uint32_t>(m_textureSets.size()) + TEXTURE_SET_ASSET_IDX_OFFSET;

		std::string iconFullPath = computeIconPath(loadingPath, 0);
		bool iconFileExists = formatIconPath(iconFullPath);

		m_textureSets.emplace_back(new AssetTextureSet(loadingPath, !iconFileExists, assetId, m_onAssetUpdatedCallback, m_materialsGPUManager, this, parentAssetId));
		m_assetAdded = true;
	}

	return assetId;
}

Wolf::ResourceNonOwner<TextureSetEditor> AssetManager::getTextureSetEditor(AssetId assetId) const
{
	if (!isTextureSet(assetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a texture set");
	}

	return m_textureSets[assetId - TEXTURE_SET_ASSET_IDX_OFFSET]->getTextureSetEditor();
}

AssetId AssetManager::addMaterial(const std::string& loadingPath, AssetId parentAssetId)
{
	if (m_materials.size() >= MAX_MATERIAL_ASSET_COUNT)
	{
		Wolf::Debug::sendCriticalError("Maximum material assets reached");
		return NO_ASSET;
	}

	AssetId assetId = NO_ASSET;

	for (uint32_t i = 0; i < m_materials.size(); ++i)
	{
		if (m_materials[i]->getLoadingPath() == loadingPath)
		{
			assetId = i + MATERIAL_ASSET_IDX_OFFSET;
			break;
		}
	}

	if (assetId == NO_ASSET)
	{
		assetId = static_cast<uint32_t>(m_materials.size()) + MATERIAL_ASSET_IDX_OFFSET;

		std::string iconFullPath = computeIconPath(loadingPath, 0);
		bool iconFileExists = formatIconPath(iconFullPath);

		m_materials.emplace_back(new AssetMaterial(loadingPath, !iconFileExists, assetId, m_onAssetUpdatedCallback, m_materialsGPUManager, this, parentAssetId));
		m_assetAdded = true;
	}

	return assetId;
}

Wolf::ResourceNonOwner<AssetMaterial> AssetManager::getAssetMaterial(AssetId assetId) const
{
	if (!isMaterial(assetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a material");
	}

	return m_materials[assetId - MATERIAL_ASSET_IDX_OFFSET].createNonOwnerResource();
}

bool AssetManager::isMaterialLoaded(AssetId assetId) const
{
	if (!isMaterial(assetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a material");
	}

	return m_materials[assetId - MATERIAL_ASSET_IDX_OFFSET]->isLoaded();
}

Wolf::ResourceNonOwner<MaterialEditor> AssetManager::getMaterialEditor(AssetId assetId) const
{
	if (!isMaterial(assetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a material");
	}

	return m_materials[assetId - MATERIAL_ASSET_IDX_OFFSET]->getEditor();
}

AssetId AssetManager::addParticle(const std::string& loadingPath)
{
	if (m_particles.size() >= MAX_PARTICLE_ASSET_COUNT)
	{
		Wolf::Debug::sendCriticalError("Maximum particle assets reached");
		return NO_ASSET;
	}

	AssetId assetId = NO_ASSET;

	for (uint32_t i = 0; i < m_particles.size(); ++i)
	{
		if (m_particles[i]->getLoadingPath() == loadingPath)
		{
			assetId = i + PARTICLE_ASSET_IDX_OFFSET;
			break;
		}
	}

	if (assetId == NO_ASSET)
	{
		assetId = static_cast<uint32_t>(m_particles.size()) + PARTICLE_ASSET_IDX_OFFSET;

		std::string iconFullPath = computeIconPath(loadingPath, 0);
		bool iconFileExists = formatIconPath(iconFullPath);

		m_particles.emplace_back(new AssetParticle(loadingPath, !iconFileExists, assetId, m_onAssetUpdatedCallback, m_materialsGPUManager, this));
		m_assetAdded = true;
	}

	return assetId;
}

Wolf::ResourceNonOwner<ParticleEditor> AssetManager::getParticleEditor(AssetId assetId) const
{
	if (!isParticle(assetId))
	{
		Wolf::Debug::sendCriticalError("AssetId is not a particle");
	}

	return m_particles[assetId - PARTICLE_ASSET_IDX_OFFSET]->getEditor();
}

AssetId AssetManager::addMesh(ExternalSceneLoader::MeshData& meshData, const std::string& name, AssetId defaultMaterialAssetId, AssetId parentAssetId)
{
	return addMeshInternal(name, meshData, defaultMaterialAssetId, parentAssetId);
}

AssetId AssetManager::addMeshInternal(const std::string& loadingPath, ExternalSceneLoader::MeshData& meshData, AssetId defaultMaterialAssetId, AssetId parentAssetId)
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
	bool iconFileExists = formatIconPath(iconFullPath);
	uint32_t thumbnailsCount = 0;
	while (iconFileExists)
	{
		thumbnailsCount++;

		std::string nextIconFullPath = computeIconPath(loadingPath, thumbnailsCount);
		iconFileExists = formatIconPath(nextIconFullPath);
	}

	iconFileExists = thumbnailsCount > 0;
	if (thumbnailsCount > 1) // We need to replace the original with the latest one
	{
		std::string originalPath = computeIconPath(loadingPath, 0);
		std::string latestPath = computeIconPath(loadingPath, thumbnailsCount - 1);

		std::filesystem::remove(originalPath);
		std::filesystem::rename(latestPath, originalPath);
	}

	AssetId assetId = static_cast<uint32_t>(m_meshes.size()) + MESH_ASSET_IDX_OFFSET;

	AssetMesh* newMesh = new AssetMesh(this, loadingPath, !iconFileExists, assetId, m_onAssetUpdatedCallback, m_bufferPoolInterface, meshData, defaultMaterialAssetId, parentAssetId,
		m_isolateMeshCallback, m_removeIsolationAndGetViewMatrixCallback, m_renderingPipeline, m_editorPushDataToGPU);
	m_meshes.emplace_back(newMesh);
	m_assetAdded = true;

	return assetId;
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

	std::string iconPathNoExtension = "UI/media/assetIcons/" + computeModelFullIdentifier(loadingPath);
	if (thumbnailsLockedCount > 0)
	{
		iconPathNoExtension += "_" + std::to_string(thumbnailsLockedCount);
	}
	std::string iconPath = iconPathNoExtension + thumbnailExtension;
	return iconPath;
}

bool AssetManager::formatIconPath(std::string& inOutIconPath)
{
	bool iconFileExists = false;
	if (const std::ifstream iconFile(inOutIconPath.c_str()); iconFile.good())
	{
		inOutIconPath = inOutIconPath.substr(3, inOutIconPath.size()); // remove "UI/"
		iconFileExists = true;
	}
	else
	{
		inOutIconPath = "media/assetIcons/no_icon.png";
	}

	return iconFileExists;
}

void AssetManager::releaseAllEditorsFromTransientEntity()
{
	if (m_transientEditionEntity)
	{
		// we don't keep the pointer, it's owned by the asset
		MeshAssetEditor* meshAssetEditor = m_transientEditionEntity->releaseComponent<MeshAssetEditor>();
		TextureSetEditor* textureSetEditor = m_transientEditionEntity->releaseComponent<TextureSetEditor>();
		ExternalSceneAssetEditor* externalSceneAssetEditor = m_transientEditionEntity->releaseComponent<ExternalSceneAssetEditor>();
		ImageEditor* imageEditor = m_transientEditionEntity->releaseComponent<ImageEditor>();
		MaterialEditor* materialEditor = m_transientEditionEntity->releaseComponent<MaterialEditor>();
		ParticleEditor* particleEditor = m_transientEditionEntity->releaseComponent<ParticleEditor>();

		m_currentAssetInEdition = NO_ASSET;
	}
}

void AssetManager::onAssetEditionChanged(Notifier::Flags flags)
{
	m_currentAssetNeedRebuildFlags |= flags;
}

bool AssetManager::saveAsset(std::stringstream& outStringStream, Wolf::ResourceNonOwner<AssetInterface> assetInterface)
{
	if (assetInterface->getParentAssetId() != NO_ASSET)
		return false;

	outStringStream << "\t\t\t{\n";
	outStringStream << "\t\t\t\t\"loadingPath\": \"" + assetInterface->getLoadingPath() + "\"\n";
	outStringStream << "\t\t\t}";

	std::ofstream outAssetFile(g_editorConfiguration->computeFullPathFromLocalPath(assetInterface->getLoadingPath()));

	std::string outJSON;
	outJSON += "{\n";

	std::vector<Wolf::ResourceNonOwner<ComponentInterface>> editors;
	assetInterface->getEditors(editors);

	for (uint32_t i = 0; i < editors.size(); ++i)
	{
		const Wolf::ResourceNonOwner<ComponentInterface>& editor = editors[i];

		outJSON += "\t" R"(")" + editor->getId() + R"(": {)" "\n";
		outJSON += "\t\t" R"("params": [)" "\n";
		editor->addParamsToJSON(outJSON, 2);
		if (const size_t commaPos = outJSON.substr(outJSON.size() - 3).find(','); commaPos != std::string::npos)
		{
			outJSON.erase(commaPos + outJSON.size() - 3);
		}
		outJSON += "\t\t]\n";
		outJSON += "\t}";
		outJSON += i == editors.size() - 1 ? "\n" : ",\n";
	}

	outJSON += "}";

	outAssetFile << outJSON;

	outAssetFile.close();

	return true;
}

void AssetManager::onAssetUpdated(AssetId assetId)
{
	// TODO
}

template <typename T>
void AssetManager::addToJSON(const T& assetList, const std::string& assetType, std::string& outJSON, uint32_t& currentCount, uint32_t offset, uint32_t maxCount, const std::string& search,
	const std::vector<std::string>& filters)
{
	if (currentCount >= maxCount)
		return;

	if (filters.empty() || (filters[0] != "all" && std::find(filters.begin(), filters.end(), assetType) == filters.end()))
		return;

	for (size_t i = 0; i < assetList.size(); ++i)
	{
		if (currentCount >= offset)
		{
			const auto& asset = assetList[i];

			std::string name = asset->getLoadingPath();
			if (!search.empty() && !name.contains(search))
				continue;

			std::string iconPath = asset->computeIconPath();
			formatIconPath(iconPath);

			outJSON += R"({ "id": ")" + std::to_string(asset->getAssetId()) +
					   R"(", "name": ")" + asset->getLoadingPath() +
					   R"(", "type": ")" + assetType +
					   R"(", "iconPath": ")" + iconPath + R"(" })";
			outJSON += ", \n";
		}

		currentCount++;
		if (currentCount >= maxCount)
			return;
	}
}

ultralight::JSValue AssetManager::requestAssetPayloadJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string inputJSON = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	Wolf::JSONReader::StringReadInfo stringReadInfo{};
	stringReadInfo.jsonData = inputJSON;
	Wolf::JSONReader jsonReader(stringReadInfo);

	std::string search = jsonReader.getRoot()->getPropertyString("search");
	uint32_t offset = static_cast<uint32_t>(jsonReader.getRoot()->getPropertyFloat("offset"));
	uint32_t maxCount = static_cast<uint32_t>(jsonReader.getRoot()->getPropertyFloat("limit")) + offset;
	std::vector<std::string> filters = jsonReader.getRoot()->getPropertyStringArray("type");

	std::string outJSON = R"({
	 	"assets": [
	)";

	uint32_t currentCount = 0;
	addToJSON(m_externalScenes, "externalScene", outJSON, currentCount, offset, maxCount, search, filters);
	addToJSON(m_meshes, "mesh", outJSON, currentCount, offset, maxCount, search, filters);
	addToJSON(m_images, "image", outJSON, currentCount, offset, maxCount, search, filters);
	addToJSON(m_textureSets, "textureSet", outJSON, currentCount, offset, maxCount, search, filters);
	addToJSON(m_materials, "material", outJSON, currentCount, offset, maxCount, search, filters);
	addToJSON(m_particles, "particle", outJSON, currentCount, offset, maxCount, search, filters);

	outJSON = outJSON.substr(0, outJSON.size() - 3);

	outJSON += "]\n}";

	return { outJSON.c_str() };
}
