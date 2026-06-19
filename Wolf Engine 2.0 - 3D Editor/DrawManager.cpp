#include "DrawManager.h"

#include "AssetManager.h"
#include "CameraList.h"
#include "CommonLayouts.h"
#include "UpdateGPUBuffersPass.h"

DrawManager::DrawManager(const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
	const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface, const Wolf::ResourceNonOwner<AssetManager>& assetManager)
	: m_instanceMeshRenderer(instanceMeshRenderer), m_updateGPUBuffersPass(renderingPipeline->getUpdateGPUBuffersPass()), m_bufferPoolInterface(bufferPoolInterface),
      m_assetManager(assetManager)
{

}

void DrawManager::addMeshesToDraw(const std::vector<DrawMeshInfo>& meshesToRender, Entity* entity)
{
	m_meshMutex.lock();

	// If the entity already registered meshes, we remove them then re-add to update data
	if (m_infoByEntities.contains(entity))
	{
		const std::vector<InfoByEntity>& infoForEntity = m_infoByEntities[entity];
		for (const InfoByEntity& info : infoForEntity)
		{
			m_instanceMeshRenderer->removeInstance(info.m_instanceIdx);
		}
		m_infoByEntities[entity].clear();
	}

	for (const DrawMeshInfo& meshToDraw : meshesToRender)
	{
		Wolf::InstanceMeshRenderer::MeshToRender meshToRender = computeMeshToRender(meshToDraw.m_meshAssetId, meshToDraw.m_pipelineSet, meshToDraw.m_perPipelineDescriptorSets);

		bool meshFound = false;
		for (uint32_t i = 0; i < m_meshesRegistered.size(); ++i)
		{
			Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_meshesRegistered[i];

			if (instancedMeshRegistered->isSame(meshToRender))
			{
				meshFound = true;
				uint32_t instanceIdx = m_instanceMeshRenderer->addInstance(instancedMeshRegistered->getMeshIdx(), meshToDraw.m_instanceData.m_transform, meshToDraw.m_instanceData.m_materialIdx,
					meshToDraw.m_instanceData.m_entityIdx, meshToRender.m_pipelineSet, meshToRender.m_perPipelineDescriptorSets);

				m_infoByEntities[entity].push_back({ i, instanceIdx });

				break;
			}
		}

		if (!meshFound)
		{
			Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_meshesRegistered.emplace_back(new InstancedMeshRegistered(meshToRender, meshToDraw.m_meshAssetId,
				m_instanceMeshRenderer));

			uint32_t instanceIdx = m_instanceMeshRenderer->addInstance(instancedMeshRegistered->getMeshIdx(), meshToDraw.m_instanceData.m_transform, meshToDraw.m_instanceData.m_materialIdx,
					meshToDraw.m_instanceData.m_entityIdx, meshToRender.m_pipelineSet, meshToRender.m_perPipelineDescriptorSets);

			m_infoByEntities[entity].push_back({ static_cast<uint32_t>(m_meshesRegistered.size()) - 1, instanceIdx });
		}
	}

	m_meshMutex.unlock();
}

void DrawManager::removeMeshesForEntity(Entity* entity)
{
	m_meshMutex.lock();

	if (m_infoByEntities.contains(entity))
	{
		const std::vector<InfoByEntity>& infoForEntity = m_infoByEntities[entity];
		for (const InfoByEntity& info : infoForEntity)
		{
			m_instanceMeshRenderer->removeInstance(info.m_instanceIdx);
		}
		m_infoByEntities[entity].clear();
	}

	m_meshMutex.unlock();
}

void DrawManager::clear()
{
	m_meshMutex.lock();

	m_meshesRegistered.clear();
	m_infoByEntities.clear();

	m_meshMutex.unlock();
}

void DrawManager::isolateEntity(Entity* entity)
{
	if (!m_infoByEntities.contains(entity))
	{
		Wolf::Debug::sendError("Trying to isolate an enregistered entity");
		return;
	}

	const std::vector<InfoByEntity>& infoForEntity = m_infoByEntities[entity];
	if (infoForEntity.empty())
	{
		Wolf::Debug::sendError("Trying to isolate an entity where info is empty");
		return;
	}

	if (infoForEntity.size() != 1)
	{
		Wolf::Debug::sendWarning("Isolated an entity doesn't have exactly 1 instance, this is not currently supported. Only the first instance will be isolated");
	}

	std::vector<Wolf::InstanceMeshRenderer::OverrideInstance> instancesForEntity;
	for (const InfoByEntity& info : infoForEntity)
	{
		instancesForEntity.emplace_back(info.m_instanceIdx);
	}
	m_instanceMeshRenderer->overrideCullingInstances(instancesForEntity);

	m_isolatedEntity = entity;
}

void DrawManager::removeIsolation()
{
	m_instanceMeshRenderer->stopOverridingCullingInstances();

	removeMeshesForEntity(m_isolatedEntity);
	m_isolatedEntity = nullptr;
}

void DrawManager::activateCameras(const Wolf::CameraList& cameraList) const
{
	m_instanceMeshRenderer->activateCameraForThisFrame(CommonCameraIndices::CAMERA_IDX_MAIN, CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH);
	m_instanceMeshRenderer->activateCameraForThisFrame(CommonCameraIndices::CAMERA_IDX_MAIN, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
	m_instanceMeshRenderer->activateCameraForThisFrame(CommonCameraIndices::CAMERA_IDX_MAIN, CommonPipelineIndices::PIPELINE_IDX_OUTPUT_IDS); // TODO: only enable if picking this frame

	uint32_t cameraCount = cameraList.getCurrentCameras().size();

	for (uint32_t cameraPipelineIdx = CommonCameraIndices::CAMERA_IDX_SHADOW_CASCADE_0; cameraPipelineIdx <= CommonCameraIndices::CAMERA_IDX_SHADOW_CASCADE_3; cameraPipelineIdx++)
	{
		if (cameraPipelineIdx < cameraCount)
		{
			m_instanceMeshRenderer->activateCameraForThisFrame(cameraPipelineIdx, CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
		}
	}

	for (uint32_t cameraPipelineIdx = CommonCameraIndices::CAMERA_IDX_FIRST_CUSTOM_RENDER_PASS; cameraPipelineIdx <= CommonCameraIndices::CAMERA_IDX_LAST_CUSTOM_RENDER_PASS; cameraPipelineIdx++)
	{
		if (cameraPipelineIdx < cameraCount)
		{
			m_instanceMeshRenderer->activateCameraForThisFrame(cameraPipelineIdx, CommonPipelineIndices::PIPELINE_IDX_CUSTOM_RENDER);
		}
	}
}

void DrawManager::updateStreaming()
{
	if (Wolf::g_configuration->getUseMeshStreaming())
	{
		std::vector<Wolf::InstanceMeshRenderer::Feedback> streamingFeedbacks;
		m_instanceMeshRenderer->swapFeedbacks(streamingFeedbacks);

		for (uint32_t i = 0; i < std::min(static_cast<uint32_t>(streamingFeedbacks.size()), 4u); i++)
		{
			Wolf::InstanceMeshRenderer::Feedback streamingFeedback = streamingFeedbacks[i];

			for (uint32_t registeredMeshIdx = 0; registeredMeshIdx < m_meshesRegistered.size(); registeredMeshIdx++)
			{
				if (m_meshesRegistered[registeredMeshIdx]->getMeshIdx() == streamingFeedback.m_meshIdx)
				{
					InstancedMeshRegistered::MemoryAllocated registeredMeshMemoryAllocated = m_meshesRegistered[registeredMeshIdx]->requestLoadingForLOD(streamingFeedback.m_lod, m_instanceMeshRenderer, m_assetManager);
					if (registeredMeshMemoryAllocated.m_vertexBufferSize < 0 || registeredMeshMemoryAllocated.m_indexBufferSize < 0) // failed to load, we need to free some space
					{
						std::vector<uint32_t>& pool = registeredMeshMemoryAllocated.m_vertexBufferSize < registeredMeshMemoryAllocated.m_indexBufferSize ? m_loadedMeshSortedIdxVertexBufferSize : m_loadedMeshSortedIdxIndexBufferSize;

						for (uint32_t loadedMeshSortedByVertexBufferSizeIdx = 0; loadedMeshSortedByVertexBufferSizeIdx < pool.size(); loadedMeshSortedByVertexBufferSizeIdx++)
						{
							const LoadedMesh& loadedMesh = m_loadedMeshes[pool[loadedMeshSortedByVertexBufferSizeIdx]];
							Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& registeredMesh = m_meshesRegistered[loadedMesh.m_registeredMeshIdx];
							if (m_instanceMeshRenderer->getLastUsedFrameIdx(registeredMesh->getMeshIdx(), loadedMesh.m_lod) < Wolf::g_runtimeContext->getCurrentCPUFrameNumber() - 10)
							{
								registeredMesh->unloadLOD(loadedMesh.m_lod, m_instanceMeshRenderer, m_assetManager);

								auto matchesCriteria = [&](uint32_t idx)
								{
									if (idx >= m_loadedMeshes.size())
										return false;

									return m_loadedMeshes[idx].m_registeredMeshIdx == loadedMesh.m_registeredMeshIdx && m_loadedMeshes[idx].m_lod == loadedMesh.m_lod;
								};

								size_t sizeBefore = m_loadedMeshSortedIdxVertexBufferSize.size();
								std::erase_if(m_loadedMeshSortedIdxVertexBufferSize, matchesCriteria);
								if (sizeBefore - 1 != m_loadedMeshSortedIdxVertexBufferSize.size())
									Wolf::Debug::sendCriticalError("Didn't delete exactly 1");
								std::erase_if(m_loadedMeshSortedIdxIndexBufferSize, matchesCriteria);

								registeredMeshMemoryAllocated.m_vertexBufferSize += loadedMesh.m_vertexBufferSize;
								registeredMeshMemoryAllocated.m_indexBufferSize += loadedMesh.m_indexBufferSize;

								if (registeredMeshMemoryAllocated.m_vertexBufferSize >= 0 && registeredMeshMemoryAllocated.m_indexBufferSize >= 0)
									break;
							}
						}

						if (registeredMeshMemoryAllocated.m_vertexBufferSize < 0 && registeredMeshMemoryAllocated.m_indexBufferSize < 0)
						{
							Wolf::Debug::sendError("Couldn't free enough space?"); // note that because of holes it could be enough actually
						}
						else
						{
							registeredMeshMemoryAllocated = m_meshesRegistered[registeredMeshIdx]->requestLoadingForLOD(streamingFeedback.m_lod, m_instanceMeshRenderer, m_assetManager);
							if (registeredMeshMemoryAllocated.m_vertexBufferSize < 0 || registeredMeshMemoryAllocated.m_indexBufferSize < 0)
							{
								Wolf::Debug::sendError("Failed to allocate memory, better luck next time?");
							}
						}
					}

					if (registeredMeshMemoryAllocated.m_indexBufferSize > 0)
					{
						m_loadedMeshes.emplace_back(registeredMeshIdx, streamingFeedback.m_lod, registeredMeshMemoryAllocated.m_vertexBufferSize, registeredMeshMemoryAllocated.m_indexBufferSize);
						uint32_t addedLoadedMeshIdx = m_loadedMeshes.size() - 1;

						for (uint32_t t : m_loadedMeshSortedIdxVertexBufferSize)
						{
							if (m_loadedMeshes[t].m_registeredMeshIdx == m_loadedMeshes.back().m_registeredMeshIdx && m_loadedMeshes[t].m_lod == m_loadedMeshes.back().m_lod)
							{
								Wolf::Debug::sendCriticalError("Mesh already loaded");
							}
						}

						auto vertexIt = std::ranges::upper_bound(m_loadedMeshSortedIdxVertexBufferSize,
						    registeredMeshMemoryAllocated.m_vertexBufferSize,
						    [&](uint32_t value, uint32_t idx)
							{
								return value > m_loadedMeshes[idx].m_vertexBufferSize;
						    }
						);
						m_loadedMeshSortedIdxVertexBufferSize.insert(vertexIt, addedLoadedMeshIdx);

						auto indexIt = std::ranges::upper_bound(m_loadedMeshSortedIdxIndexBufferSize,
						    registeredMeshMemoryAllocated.m_indexBufferSize,
							[&](uint32_t value, uint32_t idx)
							{
								return value > m_loadedMeshes[idx].m_indexBufferSize;
						    }
						);
						m_loadedMeshSortedIdxIndexBufferSize.insert(indexIt, addedLoadedMeshIdx);
					}
					break;
				}
			}
		}
	}
}

Wolf::InstanceMeshRenderer::MeshToRender DrawManager::computeMeshToRender(AssetId meshAssetId, const Wolf::ResourceNonOwner<const Wolf::PipelineSet>& pipelineSet,
                                                                          const std::array<std::vector<Wolf::DescriptorSetBindInfo>, Wolf::PipelineSet::MAX_PIPELINE_COUNT>& perPipelineDescriptorSets) const
{
	Wolf::ResourceNonOwner<AssetMesh> meshAsset = m_assetManager->getMeshAsset(meshAssetId);

	float radius = meshAsset->getBoundingSphere().getRadius();
	constexpr float quality = 1.0f;

	uint32_t lodCount = meshAsset->getDefaultSimplifiedLODCount();

	Wolf::InstanceMeshRenderer::MeshToRender meshToRenderInfo = { pipelineSet };
	meshToRenderInfo.m_boundingSphere = meshAsset->getBoundingSphere();

	AssetMesh::LOD bestLOD = meshAsset->getLOD(0, 0);
	Wolf::NullableResourceNonOwner<Wolf::Mesh> mesh = bestLOD.m_mesh;
	Wolf::InstanceMeshRenderer::MeshToRender::LOD& addedLOD = meshToRenderInfo.m_lods.emplace_back(mesh ? mesh.duplicateAs<Wolf::MeshInterface>() : Wolf::NullableResourceNonOwner<Wolf::MeshInterface>(),
		lodCount == 0 ? 10'000.0f : Wolf::InstanceMeshRenderer::computeLODDistance(radius, bestLOD.m_indexCount, quality), bestLOD.m_indexCount, bestLOD.m_clusters);

	if (lodCount == 0 && !mesh)
	{
		Wolf::Debug::sendCriticalError("Mesh must be valid if there is no LOD (as mesh becomes the lowest LOD)");
	}

	bool foundWorstDefaultLOD = true;
	for (uint32_t lodIdx = 0; lodIdx < lodCount; ++lodIdx)
	{
		AssetMesh::LOD lod = meshAsset->getLOD(lodIdx + 1, 0);

		float lodDistance = lodIdx == lodCount - 1 ? 10'000.0f : Wolf::InstanceMeshRenderer::computeLODDistance(radius, lod.m_indexCount, quality);

		Wolf::NullableResourceNonOwner<Wolf::Mesh> lodMesh = lod.m_mesh;
		meshToRenderInfo.m_lods.emplace_back(lodMesh ? lodMesh.duplicateAs<Wolf::MeshInterface>() : Wolf::NullableResourceNonOwner<Wolf::MeshInterface>(), lodDistance, lod.m_indexCount,
			lod.m_clusters);

		if (lodIdx == lodCount - 1 && !lodMesh)
		{
			foundWorstDefaultLOD = false;
		}
	}

	if (!foundWorstDefaultLOD)
	{
		uint32_t sloppyLODCount = meshAsset->getSloppySimplifiedLODCount();

		AssetMesh::LOD lod = meshAsset->getLOD(sloppyLODCount, 1);

		float lodDistance = 10'000.0f;

		Wolf::NullableResourceNonOwner<Wolf::Mesh> lodMesh = lod.m_mesh;
		meshToRenderInfo.m_lods.emplace_back(lodMesh ? lodMesh.duplicateAs<Wolf::MeshInterface>() : Wolf::NullableResourceNonOwner<Wolf::MeshInterface>(), lodDistance, lod.m_indexCount,
			lod.m_clusters);

		if (!lodMesh)
		{
			Wolf::Debug::sendCriticalError("Lowest LOD must be valid");
		}
	}

	meshToRenderInfo.m_perPipelineDescriptorSets = perPipelineDescriptorSets;

	return meshToRenderInfo;
}

DrawManager::InstancedMeshRegistered::InstancedMeshRegistered(const Wolf::InstanceMeshRenderer::MeshToRender& meshToRender, AssetId meshAssetId,
	const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer)
	: m_meshAssetId(meshAssetId), m_lowLODMesh(meshToRender.m_lods.back().m_mesh)
{
	if (!m_lowLODMesh)
	{
		Wolf::Debug::sendCriticalError("Lowest LOD must be valid");
	}
	m_meshIdx = instanceMeshRenderer->registerMesh(meshToRender);
}

DrawManager::InstancedMeshRegistered::MemoryAllocated DrawManager::InstancedMeshRegistered::requestLoadingForLOD(uint32_t lod, const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer,
	const Wolf::ResourceNonOwner<AssetManager>& assetManager) const
{
    Wolf::ResourceNonOwner<AssetMesh> meshAsset = assetManager->getMeshAsset(m_meshAssetId);
	bool meshWasAlreadyLoaded = meshAsset->getLOD(lod, 0).m_mesh;

	if (!meshWasAlreadyLoaded)
	{
		AssetMesh::LoadLODResult loadResult = meshAsset->loadLOD(lod, 0);

		if (loadResult.m_result == AssetMesh::LoadLODResult::Result::FAILED)
		{
			return MemoryAllocated{ -static_cast<int32_t>(loadResult.m_requiredSpaceForVertexBuffer), -static_cast<int32_t>(loadResult.m_requiredSpaceForIndexBuffer) };
		}
		else if (loadResult.m_result == AssetMesh::LoadLODResult::Result::ALREADY_IN_CONSTRUCTION)
		{
			return MemoryAllocated{ 0, 0 };
		}
	}

	Wolf::InstanceMeshRenderer::MeshToRender::LOD lodInfo{};
	AssetMesh::LOD lodData = meshAsset->getLOD(lod, 0, true);
	lodInfo.m_mesh = lodData.m_mesh.duplicateAs<Wolf::MeshInterface>();
	lodInfo.m_clusters = lodData.m_clusters;

	instanceMeshRenderer->registerLODData(m_meshIdx, lod, lodInfo);

	if (meshWasAlreadyLoaded)
	{
		return MemoryAllocated{ 0, 0 };
	}

	size_t indexBufferAllocatedSize = static_cast<size_t>(lodInfo.m_mesh->getIndexCount()) * sizeof(uint32_t);
	size_t vertexBufferAllocatedSize = static_cast<size_t>(lodInfo.m_mesh->getVertexCount()) * static_cast<size_t>(lodInfo.m_mesh->getVertexSize());

	if (indexBufferAllocatedSize > std::numeric_limits<int32_t>::max() || vertexBufferAllocatedSize > std::numeric_limits<int32_t>::max())
		Wolf::Debug::sendCriticalError("Index buffer or vertex buffer size too big to fit in int32");

	return MemoryAllocated{ static_cast<int32_t>(indexBufferAllocatedSize), static_cast<int32_t>(vertexBufferAllocatedSize) };
}

void DrawManager::InstancedMeshRegistered::unloadLOD(uint32_t lod, const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer, const Wolf::ResourceNonOwner<AssetManager>& assetManager)
{
	Wolf::ResourceNonOwner<AssetMesh> meshAsset = assetManager->getMeshAsset(m_meshAssetId);
	meshAsset->unloadLOD(lod, 0);

	instanceMeshRenderer->unregisterLODData(m_meshIdx, lod);
}

bool DrawManager::InstancedMeshRegistered::isSame(const Wolf::InstanceMeshRenderer::MeshToRender& otherMeshToRender) const
{
	return m_lowLODMesh == otherMeshToRender.m_lods.back().m_mesh;
}
