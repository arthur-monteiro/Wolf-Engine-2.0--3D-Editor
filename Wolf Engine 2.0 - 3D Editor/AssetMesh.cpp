#include "AssetMesh.h"

#include "AssetManager.h"
#include "EditorConfiguration.h"


AssetMesh::AssetMesh(AssetManager* assetManager, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId, const std::function<void(const std::string&, const std::string&, AssetId)>& updateAssetInUICallback,
	const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface, ExternalSceneLoader::MeshData& meshData, uint32_t defaultMaterialId, AssetId parentAssetId,const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
	const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU)
: AssetInterface(loadingPath, assetId, updateAssetInUICallback, parentAssetId), m_assetManager(assetManager), m_bufferPoolInterface(bufferPoolInterface), m_staticVertices(meshData.m_staticVertices),
  m_indices(meshData.m_indices), m_skeletonVertices(meshData.m_skeletonVertices), m_animationData(meshData.m_animationData.release()), m_materialIdx(defaultMaterialId)
{
	if (m_staticVertices.empty() && m_skeletonVertices.empty())
	{
		Wolf::Debug::sendCriticalError("Can't load a mesh without vertices");
	}

	m_meshLoadingRequested = true;
	m_thumbnailGenerationRequested = !g_editorConfiguration->getDisableThumbnailGeneration() && needThumbnailsGeneration;

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
		loadModel();
		m_meshLoadingRequested = false;
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

	uint32_t currentFrameIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber();
	for (int32_t blasToDestroyIdx = static_cast<int32_t>(m_BLASesToDestroy.size()) - 1; blasToDestroyIdx >= 0; blasToDestroyIdx--)
	{
		if (m_BLASesToDestroy[blasToDestroyIdx].second <= currentFrameIdx)
		{
			m_bottomLevelAccelerationStructures[m_BLASesToDestroy[blasToDestroyIdx].first.m_lodType][m_BLASesToDestroy[blasToDestroyIdx].first.m_lod].reset(nullptr);
			m_BLASesToDestroy.erase(m_BLASesToDestroy.begin() + blasToDestroyIdx);
		}
	}
}

void AssetMesh::forceReload(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
	const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	m_meshToKeepInMemory.reset(m_mesh.release());

	loadModel();
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
	return static_cast<bool>(m_mesh);
}

std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> AssetMesh::getDefaultSimplifiedMeshes() const
{
	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> r;
	for (uint32_t i = 0; i < m_defaultSimplifiedMeshes.size(); i++)
	{
		r.emplace_back(m_defaultSimplifiedMeshes[i].createNonOwnerResource());
	}
	return r;
}

std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> AssetMesh::getSloppySimplifiedMeshes() const
{
	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> r;
	for (uint32_t i = 0; i < m_sloppySimplifiedMeshes.size(); i++)
	{
		r.emplace_back(m_sloppySimplifiedMeshes[i].createNonOwnerResource());
	}
	return r;
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

void AssetMesh::loadMeshFormatter(Wolf::ResourceUniqueOwner<MeshFormatter>& meshFormatter)
{
	meshFormatter.reset(new MeshFormatter(m_loadingPath, m_assetManager));
	if (!meshFormatter->isMeshesLoaded())
	{
		LoadedMeshData loadedMeshData{};
		loadModelFromData(loadedMeshData);

		MeshFormatter::DataInput meshFormatterDataInput{};
		meshFormatterDataInput.m_fileName = m_loadingPath;
		meshFormatterDataInput.m_staticVertices = std::move(loadedMeshData.m_staticVertices);
		meshFormatterDataInput.m_skeletonVertices = std::move(loadedMeshData.m_skeletonVertices);
		meshFormatterDataInput.m_indices = std::move(loadedMeshData.m_indices);
		meshFormatterDataInput.m_generateDefaultLODCount = 16;
		meshFormatterDataInput.m_generateSloppyLODCount = 16;
		meshFormatterDataInput.m_animationData = std::move(loadedMeshData.m_animationData);
		meshFormatter->computeData(meshFormatterDataInput);
	}
}

void AssetMesh::loadModel()
{
	Wolf::Timer timer(std::string(m_loadingPath) + " loading");

	Wolf::ResourceUniqueOwner<MeshFormatter> meshFormatter;
	loadMeshFormatter(meshFormatter);

	VkBufferUsageFlags additionalFlags = 0;
	if (g_editorConfiguration->getEnableRayTracing())
	{
		additionalFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if (!meshFormatter->getStaticVertices().empty())
	{
		m_mesh.reset(new Wolf::Mesh(meshFormatter->getStaticVertices(), meshFormatter->getIndices(), m_bufferPoolInterface, meshFormatter->getAABB(), meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
	}
	else if (!meshFormatter->getSkeletonVertices().empty())
	{
		m_mesh.reset(new Wolf::Mesh(meshFormatter->getSkeletonVertices(), meshFormatter->getIndices(), m_bufferPoolInterface, meshFormatter->getAABB(), meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
	}
	else
	{
		Wolf::Debug::sendCriticalError("No vertex found");
	}

	m_meshAssetEditor->setIsCentered(meshFormatter->isMeshCentered());

	// LODs
	const std::vector<MeshFormatter::LODInfo>& defaultLODs = meshFormatter->getDefaultLODInfo();
	const std::vector<MeshFormatter::LODInfo>& sloppyLODs = meshFormatter->getSloppyLODInfo();

	for (const MeshFormatter::LODInfo& lod : defaultLODs)
	{
		if (!meshFormatter->getStaticVertices().empty())
		{
			m_defaultSimplifiedMeshes.emplace_back(new Wolf::Mesh(lod.m_staticVertices, lod.m_indices, m_bufferPoolInterface, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else
		{
			m_defaultSimplifiedMeshes.emplace_back(new Wolf::Mesh(lod.m_skeletonVertices, lod.m_indices, m_bufferPoolInterface, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}

		MeshAssetEditor::AddLODInfo addLodInfo{};
		addLodInfo.m_materialIdx = m_materialIdx;
		addLodInfo.m_mesh = m_defaultSimplifiedMeshes.back().createNonOwnerResource();
		addLodInfo.m_lodType = 0;
		addLodInfo.m_error = lod.m_error;
		m_meshAssetEditor->addLOD(addLodInfo);
	}

	for (const MeshFormatter::LODInfo& lod : sloppyLODs)
	{
		if (!meshFormatter->getStaticVertices().empty())
		{
			m_sloppySimplifiedMeshes.emplace_back(new Wolf::Mesh(lod.m_staticVertices, lod.m_indices, m_bufferPoolInterface, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
		}
		else
		{
			m_sloppySimplifiedMeshes.emplace_back(new Wolf::Mesh(lod.m_skeletonVertices, lod.m_indices, m_bufferPoolInterface, meshFormatter->getAABB(),
				meshFormatter->getBoundingSphere(), additionalFlags, additionalFlags));
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

	m_defaultLODsInfo = defaultLODs;
	for (MeshFormatter::LODInfo& lod : m_defaultLODsInfo)
	{
		lod.m_indices.clear();
	}
	m_sloppyLODsInfo = defaultLODs;
	for (MeshFormatter::LODInfo& lod : m_sloppyLODsInfo)
	{
		lod.m_indices.clear();
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
	std::string iconPath = AssetManager::computeIconPath(m_loadingPath, m_thumbnailCountToMaintain);

	thumbnailsGenerationPass->addRequestBeforeFrame({ getMesh(), isAnimated() ? Wolf::NullableResourceNonOwner<AnimationData>(getAnimationData()) : Wolf::NullableResourceNonOwner<AnimationData>(), m_materialIdx, iconPath,
		[this, iconPath]() { m_updateAssetInUICallback(computeName(), iconPath.substr(3, iconPath.size()), m_assetId); },
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
	const Wolf::ResourceUniqueOwner<Wolf::Mesh>* mesh = &m_mesh;
	if (lod > 0)
	{
		if (lodType == 0)
		{
			mesh = &m_defaultSimplifiedMeshes[lod - 1];
		}
		else
		{
			mesh = &m_sloppySimplifiedMeshes[lod - 1];
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