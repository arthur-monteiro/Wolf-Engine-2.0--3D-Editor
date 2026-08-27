#include "AssetMesh.h"

#include "AssetManager.h"
#include "CacheHelper.h"
#include "EditorConfiguration.h"
#include "MathsUtilsEditor.h"

AssetMesh::AssetMesh(AssetManager* assetManager, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(AssetId)>& onAssetUpdateCallback,
	const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface, ExternalSceneLoader::MeshData& meshData, AssetId defaultMaterialAssetId, AssetId parentAssetId,
	const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
	const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU)
: AssetInterface(loadingPath, assetId, onAssetUpdateCallback, parentAssetId), m_assetManager(assetManager), m_bufferPoolInterface(bufferPoolInterface), m_staticVertices(meshData.m_staticVertices),
  m_indices(meshData.m_indices), m_skeletonVertices(meshData.m_skeletonVertices), m_animationData(meshData.m_animationData.release()), m_materialAssetId(defaultMaterialAssetId),
  m_pushDataToGPUManager(editorPushDataToGPU.duplicateAs<Wolf::GPUDataTransfersManagerInterface>()), m_positionsCacheFilename(meshData.m_cachePositionFilename),
m_indicesCacheFilename(meshData.m_cacheIndicesFilename)
{
	m_meshLoadingRequested = true;
	m_thumbnailGenerationRequested = !g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration;

	if (m_thumbnailGenerationRequested && Wolf::g_configuration->getUseMeshStreaming())
	{
		Wolf::Debug::sendWarning("Can't create mesh thumbnail when mesh streaming is activated");
		m_thumbnailGenerationRequested = false;
	}

	m_meshAssetEditor.reset(new MeshAssetEditor(loadingPath, isolateMeshCallback, removeIsolationAndGetViewMatrixCallback,
		[this](const glm::mat4& viewMatrix) { requestThumbnailReload(viewMatrix); }, renderingPipeline, editorPushDataToGPU));

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

		m_thumbnailGenerationViewMatrix = viewMatrix;
	}
}

AssetMesh::~AssetMesh()
{
	m_meshAssetEditor.reset(nullptr);
}

void AssetMesh::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	if (m_meshLoadingRequested)
	{
		loadMesh();
		m_meshLoadingRequested = false;
	}
	if (m_thumbnailGenerationRequested)
	{
		if (!getLOD(0, 0).m_mesh)
		{
			loadLOD(0, 0);
			notifySubscribers();
		}

		generateThumbnail(thumbnailsGenerationPass);
		m_thumbnailGenerationRequested = false;
	}

	if (m_meshToKeepInMemory)
	{
		m_meshToKeepInMemory.reset(nullptr);
	}

	uint32_t currentFrameIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber();
	for (int32_t blasToDestroyIdx = static_cast<int32_t>(m_BLASesToDestroy.size()) - 1; blasToDestroyIdx >= 0; blasToDestroyIdx--)
	{
		if (m_BLASesToDestroy[blasToDestroyIdx].second <= currentFrameIdx)
		{
			m_bottomLevelAccelerationStructures[m_BLASesToDestroy[blasToDestroyIdx].first.m_lodType][m_BLASesToDestroy[blasToDestroyIdx].first.m_lod].reset(nullptr);
			m_BLASesToDestroy.erase(m_BLASesToDestroy.begin() + blasToDestroyIdx);
		}
	}

	for (int32_t lodInConstructionIdx = m_lodsInConstruction.size() - 1; lodInConstructionIdx >= 0; lodInConstructionIdx--)
	{
		if (m_lodsInConstruction[lodInConstructionIdx].m_buildFrameIdx + Wolf::g_configuration->getMaxCachedFrames() < currentFrameIdx)
		{
			m_lodsInConstruction.erase(m_lodsInConstruction.begin() + lodInConstructionIdx);
		}
	}
}

void AssetMesh::forceReload(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	m_meshToKeepInMemory.reset(m_mesh.m_mesh.release());

	loadMesh();
	generateThumbnail(thumbnailsGenerationPass);
	m_meshLoadingRequested = false;
}

void AssetMesh::requestThumbnailReload()
{
	std::string iconPath = AssetManager::computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);
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

bool AssetMesh::isLoaded() const
{
	return true;
}

AssetMesh::LoadLODResult AssetMesh::loadLOD(uint32_t lodIdx, uint32_t lodType)
{
	for (uint32_t lodInConstructionIdx = 0; lodInConstructionIdx < m_lodsInConstruction.size(); lodInConstructionIdx++)
	{
		if (m_lodsInConstruction[lodInConstructionIdx].m_lod == lodIdx && m_lodsInConstruction[lodInConstructionIdx].m_lodType == lodType)
		{
			return { LoadLODResult::Result::ALREADY_IN_CONSTRUCTION };
		}
	}
	m_lodsInConstruction.emplace_back(lodIdx, lodType, Wolf::g_runtimeContext->getCurrentCPUFrameNumber());

	Wolf::ResourceUniqueOwner<MeshFormatter> meshFormatter;
	loadMeshFormatter(meshFormatter, { lodIdx, lodType});

	VkBufferUsageFlags additionalFlags = 0;
	if (g_editorConfiguration->getEnableRayTracing())
	{
		additionalFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}

	LoadLODResult result { LoadLODResult::Result::REQUESTED };

	if (lodIdx == 0)
	{
		if (!meshFormatter->getStaticVertices().empty())
		{
			uint32_t requestedSizeForIndices = meshFormatter->getIndices().size() * sizeof(uint32_t);
			uint32_t requestedSizeForVertices = meshFormatter->getStaticVertices().size() * sizeof(Vertex3D);

			if (!m_bufferPoolInterface->hasEnoughSpace(requestedSizeForIndices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | additionalFlags, sizeof(uint32_t)))
			{
				result.m_result = LoadLODResult::Result::FAILED;
				result.m_requiredSpaceForIndexBuffer = requestedSizeForIndices;
			}

			if (!m_bufferPoolInterface->hasEnoughSpace(requestedSizeForVertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | additionalFlags, sizeof(Vertex3D)))
			{
				result.m_result = LoadLODResult::Result::FAILED;
				result.m_requiredSpaceForVertexBuffer = requestedSizeForVertices;
			}

			if (result.m_result == LoadLODResult::Result::FAILED)
			{
				return result;
			}

			m_mesh.m_mesh.reset(new Wolf::Mesh(meshFormatter->getStaticVertices(), meshFormatter->getIndices(), m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else if (!meshFormatter->getSkeletonVertices().empty())
		{
			m_mesh.m_mesh.reset(new Wolf::Mesh(meshFormatter->getSkeletonVertices(), meshFormatter->getIndices(), m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else
		{
			Wolf::Debug::sendCriticalError("No vertex found");
		}
	}
	else
	{
		const MeshFormatter::LODInfo& lod = lodType == 0 ? meshFormatter->getDefaultLODInfo()[lodIdx - 1] : meshFormatter->getSloppyLODInfo()[lodIdx - 1];

		Wolf::ResourceUniqueOwner<InternalLOD>& internalLOD = m_defaultSimplifiedMeshes[lodIdx - 1];

		if (!lod.m_staticVertices.empty())
		{
			uint32_t requestedSizeForIndices = lod.m_indices.size() * sizeof(uint32_t);
			uint32_t requestedSizeForVertices = lod.m_staticVertices.size() * sizeof(Vertex3D);

			if (!m_bufferPoolInterface->hasEnoughSpace(requestedSizeForIndices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | additionalFlags, sizeof(uint32_t)))
			{
				result.m_result = LoadLODResult::Result::FAILED;
				result.m_requiredSpaceForIndexBuffer = requestedSizeForIndices;
			}

			if (!m_bufferPoolInterface->hasEnoughSpace(requestedSizeForVertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | additionalFlags, sizeof(Vertex3D)))
			{
				result.m_result = LoadLODResult::Result::FAILED;
				result.m_requiredSpaceForVertexBuffer = requestedSizeForVertices;
			}

			if (result.m_result == LoadLODResult::Result::FAILED)
			{
				return result;
			}

			internalLOD->m_mesh.reset(new Wolf::Mesh(lod.m_staticVertices, lod.m_indices, m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else if (!lod.m_skeletonVertices.empty())
		{
			internalLOD->m_mesh.reset(new Wolf::Mesh(lod.m_skeletonVertices, lod.m_indices, m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else
		{
			Wolf::Debug::sendCriticalError("LOD don't have any vertex");
		}
	}

	return result;
}

void AssetMesh::unloadLOD(uint32_t lodIdx, uint32_t lodType)
{
	if (lodIdx == 0)
	{
		m_mesh.m_mesh.reset(nullptr);
	}
	else
	{
		if (lodType == 0)
		{
			m_defaultSimplifiedMeshes[lodIdx - 1]->m_mesh.reset(nullptr);
		}
		else
		{
			m_sloppySimplifiedMeshes[lodIdx - 1]->m_mesh.reset(nullptr);
		}
	}

	for (int32_t lodInConstructionIdx = m_lodsInConstruction.size() - 1; lodInConstructionIdx >= 0; lodInConstructionIdx--)
	{
		if (m_lodsInConstruction[lodInConstructionIdx].m_lod == lodIdx && m_lodsInConstruction[lodInConstructionIdx].m_lodType == lodType)
		{
			m_lodsInConstruction.erase(m_lodsInConstruction.begin() + lodInConstructionIdx);
		}
	}
}

AssetMesh::LOD AssetMesh::getLOD(uint32_t lod, uint32_t lodType, bool ignoreDelayForLODInConstruction)
{
	Wolf::NullableResourceNonOwner<Wolf::Mesh> mesh;
	uint32_t indexCount = 0;

	if (lod == 0)
	{
		if (m_mesh.m_mesh)
			mesh = m_mesh.m_mesh.createNonOwnerResource();
		indexCount = m_mesh.m_indexCount;
	}
	else if (lodType == 0)
	{
		if (m_defaultSimplifiedMeshes[lod - 1]->m_mesh)
			mesh = m_defaultSimplifiedMeshes[lod - 1]->m_mesh.createNonOwnerResource();
		indexCount = m_defaultSimplifiedMeshes[lod - 1]->m_indexCount;
	}
	else if (lodType == 1)
	{
		if (m_sloppySimplifiedMeshes[lod - 1]->m_mesh)
			mesh = m_sloppySimplifiedMeshes[lod - 1]->m_mesh.createNonOwnerResource();
		indexCount = m_sloppySimplifiedMeshes[lod - 1]->m_indexCount;
	}
	else
	{
		Wolf::Debug::sendCriticalError("Unsupported LOD type");
	}

	if (!ignoreDelayForLODInConstruction)
	{
		for (uint32_t lodInConstructionIdx = 0; lodInConstructionIdx < m_lodsInConstruction.size(); lodInConstructionIdx++)
		{
			if (m_lodsInConstruction[lodInConstructionIdx].m_lod == lod && m_lodsInConstruction[lodInConstructionIdx].m_lodType == lodType)
			{
				mesh = Wolf::NullableResourceNonOwner<Wolf::Mesh>();
			}
		}
	}

	return { mesh, indexCount };
}

Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> AssetMesh::getBLAS(uint32_t lod, uint32_t lodType)
{
	if (lod == 0)
	{
		lodType = 0; // LOD 0 is only generated for LOD type 0
	}

	if (lodType >= m_bottomLevelAccelerationStructures.size() || lod >= m_bottomLevelAccelerationStructures[lodType].size())
	{
		return Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure>();
	}

	ensureBLASIsLoaded(lod, lodType);

	return m_bottomLevelAccelerationStructures[lodType][lod].createNonOwnerResource();
}

MeshFormatter* AssetMesh::computeMeshFormatter()
{
	Wolf::ResourceUniqueOwner<MeshFormatter> meshFormatter;
	loadMeshFormatter(meshFormatter);

	return meshFormatter.release();
}

void AssetMesh::loadMeshFormatter(Wolf::ResourceUniqueOwner<MeshFormatter>& meshFormatter, MeshFormatter::ReadSpecificLODInfo readSpecificLODInfo)
{
	meshFormatter.reset(new MeshFormatter(m_loadingPath, m_assetManager, !Wolf::g_configuration->getUseMeshStreaming(), readSpecificLODInfo));
	if (!meshFormatter->isMeshLoaded())
	{
		if (m_staticVertices.empty() && m_skeletonVertices.empty() && m_positionsCacheFilename.empty())
		{
			Wolf::Debug::sendCriticalError("Can't load a mesh without vertices");
		}
		if (m_indices.empty() && m_indicesCacheFilename.empty())
		{
			Wolf::Debug::sendCriticalError("Can't load a mesh without indices");
		}

		LoadedMeshData loadedMeshData{};
		loadModelFromData(loadedMeshData);

		MeshFormatter::DataInput meshFormatterDataInput{};
		meshFormatterDataInput.m_fileName = m_loadingPath;
		meshFormatterDataInput.m_staticVertices = std::move(loadedMeshData.m_staticVertices);
		if (!m_positionsCacheFilename.empty())
		{
			std::ifstream file(m_positionsCacheFilename, std::ios::binary);
			if (!file.is_open())
			{
				Wolf::Debug::sendCriticalError("Cannot open file for reading");
			}

			std::vector<glm::vec3> positions;
			CacheHelper::readVector(file, positions);

			file.close();

			meshFormatterDataInput.m_staticVertices.resize(positions.size());
			for (uint32_t i = 0; i < positions.size(); i++)
			{
				meshFormatterDataInput.m_staticVertices[i].pos = positions[i];
			}

			meshFormatterDataInput.m_recomputeNormals = true;
		}
		meshFormatterDataInput.m_skeletonVertices = std::move(loadedMeshData.m_skeletonVertices);
		meshFormatterDataInput.m_indices = std::move(loadedMeshData.m_indices);
		if (!m_indicesCacheFilename.empty())
		{
			std::ifstream file(m_indicesCacheFilename, std::ios::binary);
			if (!file.is_open())
			{
				Wolf::Debug::sendCriticalError("Cannot open file for reading");
			}

			CacheHelper::readVector(file, meshFormatterDataInput.m_indices);

			file.close();
		}
		meshFormatterDataInput.m_generateDefaultLODCount = 16;
		meshFormatterDataInput.m_generateSloppyLODCount = 16;
		meshFormatterDataInput.m_animationData = std::move(loadedMeshData.m_animationData);
		meshFormatter->computeData(meshFormatterDataInput);

		if (Wolf::g_configuration->getUseMeshStreaming())
		{
			meshFormatter.reset(nullptr);
			meshFormatter.reset(new MeshFormatter(m_loadingPath, m_assetManager, !Wolf::g_configuration->getUseMeshStreaming(), readSpecificLODInfo));
		}
	}
}

void AssetMesh::loadMesh()
{
	Wolf::Timer timer(std::string(m_loadingPath) + " loading");

	Wolf::ResourceUniqueOwner<MeshFormatter> meshFormatter;
	loadMeshFormatter(meshFormatter);

	m_boundingBox = meshFormatter->getAABB();
	m_boundingSphere = meshFormatter->getBoundingSphere();

	m_mesh.m_indexCount = meshFormatter->getIndexCount();
	VkBufferUsageFlags additionalFlags = 0;
	if (g_editorConfiguration->getEnableRayTracing())
	{
		additionalFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if (!meshFormatter->getStaticVertices().empty())
	{
		m_mesh.m_mesh.reset(new Wolf::Mesh(meshFormatter->getStaticVertices(), meshFormatter->getIndices(), m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
			meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
	}
	else if (!meshFormatter->getSkeletonVertices().empty())
	{
		m_mesh.m_mesh.reset(new Wolf::Mesh(meshFormatter->getSkeletonVertices(), meshFormatter->getIndices(), m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
			meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
	}
	else if (!Wolf::g_configuration->getUseMeshStreaming())
	{
		Wolf::Debug::sendCriticalError("No vertex found");
	}

	m_meshAssetEditor->setIsCentered(meshFormatter->isMeshCentered());

	// LODs
	const std::vector<MeshFormatter::LODInfo>& defaultLODs = meshFormatter->getDefaultLODInfo();
	const std::vector<MeshFormatter::LODInfo>& sloppyLODs = meshFormatter->getSloppyLODInfo();

	bool foundWorstDefaultLOD = true;
	for (uint32_t lodIdx = 0; lodIdx < defaultLODs.size(); ++lodIdx)
	{
		const MeshFormatter::LODInfo& lod = defaultLODs[lodIdx];

		Wolf::ResourceUniqueOwner<InternalLOD>& internalLOD = m_defaultSimplifiedMeshes.emplace_back(new InternalLOD());
		internalLOD->m_indexCount = lod.m_indexCount;
		if (!lod.m_staticVertices.empty())
		{
			internalLOD->m_mesh.reset(new Wolf::Mesh(lod.m_staticVertices, lod.m_indices, m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else if (!lod.m_skeletonVertices.empty())
		{
			internalLOD->m_mesh.reset(new Wolf::Mesh(lod.m_skeletonVertices, lod.m_indices, m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else if (!Wolf::g_configuration->getUseMeshStreaming() || lodIdx == defaultLODs.size() - 1)
		{
			foundWorstDefaultLOD = false;
		}

		MeshAssetEditor::AddLODInfo addLodInfo{};
		addLodInfo.m_materialIdx = m_materialAssetId == NO_ASSET ? 0 : m_assetManager->getMaterialEditor(m_materialAssetId)->getMaterialGPUIdx();
		addLodInfo.m_indexCount = m_defaultSimplifiedMeshes.back()->m_indexCount;
		if (m_defaultSimplifiedMeshes.back()->m_mesh)
		{
			addLodInfo.m_mesh = m_defaultSimplifiedMeshes.back()->m_mesh.createNonOwnerResource();
		}
		addLodInfo.m_lodType = 0;
		addLodInfo.m_error = lod.m_error;
		m_meshAssetEditor->addLOD(addLodInfo);
	}

	for (uint32_t lodIdx = 0; lodIdx < sloppyLODs.size(); ++lodIdx)
	{
		const MeshFormatter::LODInfo& lod = sloppyLODs[lodIdx];

		Wolf::ResourceUniqueOwner<InternalLOD>& internalLOD = m_sloppySimplifiedMeshes.emplace_back(new InternalLOD());
		internalLOD->m_indexCount = lod.m_indexCount;
		if (!lod.m_staticVertices.empty())
		{
			internalLOD->m_mesh.reset(new Wolf::Mesh(lod.m_staticVertices, lod.m_indices, m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else if (!lod.m_skeletonVertices.empty())
		{
			internalLOD->m_mesh.reset(new Wolf::Mesh(lod.m_skeletonVertices, lod.m_indices, m_bufferPoolInterface, m_pushDataToGPUManager, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else if (!Wolf::g_configuration->getUseMeshStreaming() || (lodIdx == sloppyLODs.size() - 1 && !foundWorstDefaultLOD))
		{
			Wolf::Debug::sendCriticalError("Worst default LOD or sloppy LOD must be valid");
		}
	}

	if (g_editorConfiguration->getEnableRayTracing())
	{
		m_bottomLevelAccelerationStructures.resize(2);
		for (uint32_t lodType = 0; lodType < 2; lodType++)
		{
			uint32_t lodCount = lodType == 0 ? m_defaultSimplifiedMeshes.size() : m_sloppySimplifiedMeshes.size();
			m_bottomLevelAccelerationStructures[lodType].resize(lodCount + 1);
		}
	}

	m_loadedBLAS = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

	if (meshFormatter->getAnimationData())
	{
		m_animationData.reset(new AnimationData());
		*m_animationData = *meshFormatter->getAnimationData();
	}

	m_isCentered = meshFormatter->isMeshCentered();
	computeThumbnailGenerationViewMatrix(meshFormatter->getAABB());
}

void AssetMesh::loadModelFromData(LoadedMeshData& outLoadedMeshData)
{
	outLoadedMeshData.m_staticVertices = std::move(m_staticVertices);
	outLoadedMeshData.m_skeletonVertices = std::move(m_skeletonVertices);
	outLoadedMeshData.m_indices = std::move(m_indices);
}

void AssetMesh::computeThumbnailGenerationViewMatrix(const Wolf::AABB& aabb)
{
	float entityHeight = aabb.getMax().y - aabb.getMin().y;
	glm::vec3 position = aabb.getCenter() + glm::vec3(-entityHeight, entityHeight, -entityHeight);
	glm::vec3 target = aabb.getCenter();
	m_thumbnailGenerationViewMatrix = glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void AssetMesh::generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	if (Wolf::g_configuration->getUseMeshStreaming())
	{
		Wolf::Debug::sendError("Can't create mesh thumbnail when mesh streaming is activated");
	}

	std::string iconPath = computeIconPath();

	uint32_t materialGPUIdx = m_materialAssetId == NO_ASSET ? 0 : m_assetManager->getMaterialEditor(m_materialAssetId)->getMaterialGPUIdx();

	thumbnailsGenerationPass->addRequestBeforeFrame({ m_mesh.m_mesh.createNonOwnerResource(), isAnimated() ? Wolf::NullableResourceNonOwner<AnimationData>(getAnimationData()) : Wolf::NullableResourceNonOwner<AnimationData>(), materialGPUIdx, iconPath,
		[this]() { m_updateAssetInUICallback(m_assetId); },
			m_thumbnailGenerationViewMatrix });
}

void AssetMesh::requestThumbnailReload(const glm::mat4& viewMatrix)
{
	m_thumbnailGenerationViewMatrix = viewMatrix;
	requestThumbnailReload();
}

void AssetMesh::ensureBLASIsLoaded(uint32_t lod, uint32_t lodType)
{
	if (m_bottomLevelAccelerationStructures[lodType][lod])
		return;

	if (m_loadedBLAS.m_lodType != -1)
	{
		m_BLASesToDestroy.emplace_back(m_loadedBLAS, Wolf::g_runtimeContext->getCurrentCPUFrameNumber() + Wolf::g_configuration->getMaxCachedFrames());
	}

	if (g_editorConfiguration->getEnableRayTracing())
	{
		buildBLAS(lod, lodType, m_loadingPath);
		m_loadedBLAS = { lodType, lod };
	}
	else
	{
		Wolf::Debug::sendCriticalError("Can't build BLAS is ray tracing isn't enabled");
	}
}

void AssetMesh::buildBLAS(uint32_t lod, uint32_t lodType, const std::string& filename)
{
	const Wolf::ResourceUniqueOwner<Wolf::Mesh>* mesh = &m_mesh.m_mesh;
	if (lod > 0)
	{
		if (lodType == 0)
		{
			mesh = &m_defaultSimplifiedMeshes[lod - 1]->m_mesh;
		}
		else
		{
			mesh = &m_sloppySimplifiedMeshes[lod - 1]->m_mesh;
		}
	}

	Wolf::GeometryInfo geometryInfo;
	geometryInfo.mesh.vertexBuffer = &*(*mesh)->getVertexBuffer();
	geometryInfo.mesh.vertexBufferOffset = (*mesh)->getVertexBufferOffset();
	geometryInfo.mesh.vertexCount = (*mesh)->getVertexCount();
	geometryInfo.mesh.vertexSize = (*mesh)->getVertexSize();
	geometryInfo.mesh.vertexFormat = Wolf::Format::R32G32B32_SFLOAT;
	geometryInfo.mesh.indexBuffer = &*(*mesh)->getIndexBuffer();
	geometryInfo.mesh.indexBufferOffset = (*mesh)->getIndexBufferOffset();
	geometryInfo.mesh.indexCount = (*mesh)->getIndexCount();

	Wolf::BottomLevelAccelerationStructureCreateInfo createInfo{};
	createInfo.geometryInfos = { &geometryInfo, 1 };
	createInfo.buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	createInfo.name = "filename " + filename + ", lod type " + std::to_string(lodType) + ", lod " + std::to_string(lod);

	m_bottomLevelAccelerationStructures[lodType][lod].reset(Wolf::BottomLevelAccelerationStructure::createBottomLevelAccelerationStructure(createInfo));
}