#include "DrawManager.h"

#include <unordered_set>

#include "AssetManager.h"
#include "CameraList.h"
#include "CommonLayouts.h"
#include "DrawIdsPass.h"
#include "UpdateGPUBuffersPass.h"

DrawManager::DrawManager(const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
	const Wolf::ResourceNonOwner<Wolf::BufferPoolInterface>& bufferPoolInterface, const Wolf::ResourceNonOwner<AssetManager>& assetManager)
	: m_instanceMeshRenderer(instanceMeshRenderer), m_updateGPUBuffersPass(renderingPipeline->getUpdateGPUBuffersPass()), m_drawIdsPass(renderingPipeline->getDrawIdsPass()), m_bufferPoolInterface(bufferPoolInterface),
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
	if (m_drawIdsPass->isEnabledThisFrame())
	{
		m_instanceMeshRenderer->activateCameraForThisFrame(CommonCameraIndices::CAMERA_IDX_MAIN, CommonPipelineIndices::PIPELINE_IDX_OUTPUT_IDS);
	}

	// TODO: don't rely on camera count
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
	PROFILE_FUNCTION

	if (Wolf::g_configuration->getUseMeshStreaming() && !Wolf::g_configuration->getUseMeshlets())
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
					InstancedMeshRegistered::MemoryAllocated registeredMeshMemoryAllocated = m_meshesRegistered[registeredMeshIdx]->requestLoadingForLOD(streamingFeedback.m_lodOrMeshletGroupId, m_instanceMeshRenderer, m_assetManager);
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
							registeredMeshMemoryAllocated = m_meshesRegistered[registeredMeshIdx]->requestLoadingForLOD(streamingFeedback.m_lodOrMeshletGroupId, m_instanceMeshRenderer, m_assetManager);
							if (registeredMeshMemoryAllocated.m_vertexBufferSize < 0 || registeredMeshMemoryAllocated.m_indexBufferSize < 0)
							{
								Wolf::Debug::sendError("Failed to allocate memory, better luck next time?");
							}
						}
					}

					if (registeredMeshMemoryAllocated.m_indexBufferSize > 0)
					{
						m_loadedMeshes.emplace_back(registeredMeshIdx, streamingFeedback.m_lodOrMeshletGroupId, registeredMeshMemoryAllocated.m_vertexBufferSize, registeredMeshMemoryAllocated.m_indexBufferSize);
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
	else if (Wolf::g_configuration->getUseMeshStreaming() && Wolf::g_configuration->getUseMeshlets())
	{
		struct NeededMeshlet
		{
			uint32_t m_registeredMeshIdx;
			uint32_t m_clusterIdx;
			uint32_t m_meshletIdx;
		};
		std::vector<NeededMeshlet> neededMeshletsIndices;

		{
			PROFILE_SCOPED("Find needed meshlets")

			std::vector<Wolf::InstanceMeshRenderer::Feedback> streamingFeedbacks;
			m_instanceMeshRenderer->swapFeedbacks(streamingFeedbacks);

			for (uint32_t i = 0; i < streamingFeedbacks.size(); i++)
			{
				Wolf::InstanceMeshRenderer::Feedback streamingFeedback = streamingFeedbacks[i];

				uint32_t registeredMeshIdx = 0;
				for (; registeredMeshIdx < m_meshesRegistered.size(); registeredMeshIdx++)
				{
					if (m_meshesRegistered[registeredMeshIdx]->getMeshIdx() == streamingFeedback.m_meshIdx)
					{
						break;
					}
				}

				if (registeredMeshIdx == m_meshesRegistered.size())
				{
					Wolf::Debug::sendCriticalError("Mesh not registered");
				}

				Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_meshesRegistered[registeredMeshIdx];
				Wolf::ResourceNonOwner<AssetMesh> meshAsset = m_assetManager->getMeshAsset(instancedMeshRegistered->getMeshAssetId());

				const Wolf::DynamicResourceUniqueOwnerArray<AssetMesh::Cluster, 128>& clusters = meshAsset->getClusters();

				struct GroupToProcess
				{
					uint32_t m_clusterIdx;
					uint32_t m_groupIdx;

					uint64_t getKey() const
					{
						return (static_cast<uint64_t>(m_clusterIdx) << 32) | m_groupIdx;
					}
				};

				std::vector<GroupToProcess> groupsToProcess;
				std::unordered_set<uint64_t> visitedGroups;

				uint32_t groupId = streamingFeedback.m_lodOrMeshletGroupId;
				uint32_t clusterIdx = groupId >> 16;
				uint32_t groupIdxInCluster = groupId & 0xffff;

				groupsToProcess.push_back({ .m_clusterIdx = clusterIdx, .m_groupIdx = groupIdxInCluster });

				while (!groupsToProcess.empty())
				{
					GroupToProcess currentGroupToProcess = groupsToProcess.back();
					groupsToProcess.pop_back();

					if (visitedGroups.contains(currentGroupToProcess.getKey()))
						continue;

					visitedGroups.insert(currentGroupToProcess.getKey());

					const Wolf::ResourceUniqueOwner<AssetMesh::MeshletGroup>& group = clusters[currentGroupToProcess.m_clusterIdx]->m_meshletGroups[currentGroupToProcess.m_groupIdx];
					for (uint32_t meshletChildIdx : group->m_childrenMeshletIndices)
					{
						if (meshletChildIdx != -1)
						{
							neededMeshletsIndices.push_back({ .m_registeredMeshIdx = registeredMeshIdx, .m_clusterIdx = currentGroupToProcess.m_clusterIdx, .m_meshletIdx = meshletChildIdx });
						}
					}

					for (const MeshFormatter::MeshletGroup::ParentMeshlet& parentMeshlet : group->m_parentMeshlets)
					{
						if (parentMeshlet.m_clusterIdx != -1)
						{
							const Wolf::ResourceUniqueOwner<AssetMesh::Meshlet>& meshlet = clusters[parentMeshlet.m_clusterIdx]->m_meshlets[parentMeshlet.m_meshletIdx];
							uint32_t parentGroupId = meshlet->m_groupIdAsChild;

							if (parentGroupId != -1)
							{
								uint32_t parentClusterIdx = parentGroupId >> 16;
								uint32_t parentGroupIdxInCluster = parentGroupId & 0xffff;

								if (parentClusterIdx != parentMeshlet.m_clusterIdx)
								{
									Wolf::Debug::sendCriticalError("Missmatch between parent cluster id registered in group and in meshlet");
								}

								groupsToProcess.push_back({ .m_clusterIdx = parentMeshlet.m_clusterIdx, .m_groupIdx = parentGroupIdxInCluster });
							}
						}
					}
				}
			}
		}

		{
			PROFILE_SCOPED("Remove loaded meshlets")

			std::erase_if(neededMeshletsIndices, [this](const NeededMeshlet& neededMeshlet)
			{
				const Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_meshesRegistered[neededMeshlet.m_registeredMeshIdx];
				Wolf::ResourceNonOwner<AssetMesh> meshAsset = m_assetManager->getMeshAsset(instancedMeshRegistered->getMeshAssetId());

				return static_cast<bool>(meshAsset->getClusters()[neededMeshlet.m_clusterIdx]->m_meshlets[neededMeshlet.m_meshletIdx]->m_mesh);
			});
		}

		{
			PROFILE_SCOPED("Sort")

			std::ranges::sort(neededMeshletsIndices, [this](const NeededMeshlet& a, const NeededMeshlet& b)
			{
				auto getMeshletInfo = [this](uint32_t registeredMeshIdx, uint32_t clusterIdx, uint32_t meshletIdx)
				{
					const Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_meshesRegistered[registeredMeshIdx];
					Wolf::ResourceNonOwner<AssetMesh> meshAsset = m_assetManager->getMeshAsset(instancedMeshRegistered->getMeshAssetId());
					const Wolf::ResourceUniqueOwner<AssetMesh::Meshlet>& meshlet = meshAsset->getClusters()[clusterIdx]->m_meshlets[meshletIdx];

					uint32_t groupId = meshlet->m_groupIdAsChild;
					uint32_t clusterIdxFromGroupId = groupId >> 16;
					uint32_t groupIdxInCluster = groupId & 0xffff;

					if (clusterIdxFromGroupId != clusterIdx)
					{
						Wolf::Debug::sendCriticalError("Missmatch");
					}

					uint32_t level = meshAsset->getClusters()[clusterIdx]->m_meshletGroups[groupIdxInCluster]->m_hierarchyLevel;
					float lodError = meshlet->m_lodError;

					return std::pair{ level, lodError };
				};

				auto [levelA, errorA] = getMeshletInfo(a.m_registeredMeshIdx, a.m_clusterIdx, a.m_meshletIdx);
				auto [levelB, errorB] = getMeshletInfo(b.m_registeredMeshIdx, b.m_clusterIdx, b.m_meshletIdx);

				if (levelA != levelB)
				{
					return levelA > levelB;
				}

				return errorA > errorB;
			});
		}

		{
			PROFILE_SCOPED("Read files")

			static size_t MAX_UPLOAD_COUNT = 128;
			uint32_t uploadCount = 0;
			uint32_t i = 0;
			while (uploadCount < MAX_UPLOAD_COUNT && i < neededMeshletsIndices.size())
			{
				NeededMeshlet& neededMeshlet = neededMeshletsIndices[i++];

				Wolf::ResourceUniqueOwner<InstancedMeshRegistered>& instancedMeshRegistered = m_meshesRegistered[neededMeshlet.m_registeredMeshIdx];
				uint32_t poolIdx = instancedMeshRegistered->requestLoadingForMeshlet(neededMeshlet.m_clusterIdx, neededMeshlet.m_meshletIdx, m_instanceMeshRenderer, m_assetManager);

				if (poolIdx != -1)
					uploadCount++;
			}
		}
	}
}

Wolf::InstanceMeshRenderer::MeshToRender DrawManager::computeMeshToRender(AssetId meshAssetId, const Wolf::ResourceNonOwner<const Wolf::PipelineSet>& pipelineSet,
	const std::array<std::vector<Wolf::DescriptorSetBindInfo>, Wolf::PipelineSet::MAX_PIPELINE_COUNT>& perPipelineDescriptorSets) const
{
	Wolf::ResourceNonOwner<AssetMesh> meshAsset = m_assetManager->getMeshAsset(meshAssetId);

	Wolf::InstanceMeshRenderer::MeshToRender meshToRenderInfo = { pipelineSet };
	meshToRenderInfo.m_boundingSphere = meshAsset->getBoundingSphere();
	meshToRenderInfo.m_AABB = meshAsset->getBoundingBox();
	meshToRenderInfo.m_perPipelineDescriptorSets = perPipelineDescriptorSets;

	if (Wolf::g_configuration->getUseMeshlets())
	{
		const Wolf::DynamicResourceUniqueOwnerArray<AssetMesh::Cluster, 128>& clusters = meshAsset->getClusters();
		meshToRenderInfo.m_clusters.resize(clusters.size());
		for (uint32_t clusterIdx = 0; clusterIdx < clusters.size(); ++clusterIdx)
		{
			const AssetMesh::Cluster& sourceCluster = *clusters[clusterIdx];
			Wolf::InstanceMeshRenderer::MeshToRender::Cluster& targetCluster = meshToRenderInfo.m_clusters[clusterIdx];

			targetCluster.m_meshlets.resize(sourceCluster.m_meshlets.size());
			for (uint32_t meshletIdx = 0; meshletIdx < sourceCluster.m_meshlets.size(); ++meshletIdx)
			{
				const AssetMesh::Meshlet& meshlet = *sourceCluster.m_meshlets[meshletIdx];
				Wolf::InstanceMeshRenderer::MeshToRender::Meshlet& targetMeshlet = targetCluster.m_meshlets[meshletIdx];
				targetMeshlet.m_mesh = meshlet.m_mesh ? Wolf::NullableResourceNonOwner<Wolf::MeshInterface>(meshlet.m_mesh.createNonOwnerResource<Wolf::MeshInterface>()) : Wolf::NullableResourceNonOwner<Wolf::MeshInterface>();
				targetMeshlet.m_boundingSphere = meshlet.m_boundingSphere;
				targetMeshlet.m_groupIdAsParent = meshlet.m_groupIdAsParent;
				targetMeshlet.m_groupIdAsChild = meshlet.m_groupIdAsChild;
				targetMeshlet.m_parentGroupBoundingSphere = meshlet.m_parentGroupBoundingSphere;
				targetMeshlet.m_lodError = meshlet.m_lodError;
				targetMeshlet.m_parentLodError = meshlet.m_parentLodError;
				targetMeshlet.m_coneAxis[0] = meshlet.m_coneAxis[0];
				targetMeshlet.m_coneAxis[1] = meshlet.m_coneAxis[1];
				targetMeshlet.m_coneAxis[2] = meshlet.m_coneAxis[2];
				targetMeshlet.m_coneCutoff = meshlet.m_coneCutoff;
			}

			targetCluster.m_meshletGroups.resize(sourceCluster.m_meshletGroups.size());
			for (uint32_t groupIdx = 0; groupIdx < sourceCluster.m_meshletGroups.size(); ++groupIdx)
			{
				const AssetMesh::MeshletGroup& group = *sourceCluster.m_meshletGroups[groupIdx];
				Wolf::InstanceMeshRenderer::MeshToRender::MeshletGroup& targetGroup = targetCluster.m_meshletGroups[groupIdx];
				targetGroup.m_boundingSphere = group.m_boundingSphere;
				targetGroup.m_childrenMeshletIndices = group.m_childrenMeshletIndices;
				targetGroup.m_hierarchyLevel = group.m_hierarchyLevel;
			}
		}
	}
	else
	{
		float radius = meshAsset->getBoundingSphere().getRadius();
		constexpr float quality = 1.0f;

		uint32_t lodCount = meshAsset->getDefaultSimplifiedLODCount();

		AssetMesh::LOD bestLOD = meshAsset->getLOD(0, 0);
		Wolf::NullableResourceNonOwner<Wolf::Mesh> mesh = bestLOD.m_mesh;
		meshToRenderInfo.m_lods.emplace_back(mesh ? mesh.duplicateAs<Wolf::MeshInterface>() : Wolf::NullableResourceNonOwner<Wolf::MeshInterface>(),
			lodCount == 0 ? 10'000.0f : Wolf::InstanceMeshRenderer::computeLODDistance(radius, bestLOD.m_indexCount, quality), bestLOD.m_indexCount);

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
			meshToRenderInfo.m_lods.emplace_back(lodMesh ? lodMesh.duplicateAs<Wolf::MeshInterface>() : Wolf::NullableResourceNonOwner<Wolf::MeshInterface>(), lodDistance, lod.m_indexCount);

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
			meshToRenderInfo.m_lods.emplace_back(lodMesh ? lodMesh.duplicateAs<Wolf::MeshInterface>() : Wolf::NullableResourceNonOwner<Wolf::MeshInterface>(), lodDistance, lod.m_indexCount);

			if (!lodMesh)
			{
				Wolf::Debug::sendCriticalError("Lowest LOD must be valid");
			}
		}

	}

	return meshToRenderInfo;
}

DrawManager::InstancedMeshRegistered::InstancedMeshRegistered(const Wolf::InstanceMeshRenderer::MeshToRender& meshToRender, AssetId meshAssetId,
	const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer)
	: m_meshAssetId(meshAssetId)
{
	if (Wolf::g_configuration->getUseMeshlets())
	{
		m_lowLODMesh = meshToRender.m_clusters[0].m_meshlets[0].m_mesh;

		if (!m_lowLODMesh)
		{
			Wolf::Debug::sendCriticalError("No meshlet found");
		}
	}
	else
	{
		m_lowLODMesh = meshToRender.m_lods.back().m_mesh;

		if (!m_lowLODMesh)
		{
			Wolf::Debug::sendCriticalError("Lowest LOD must be valid");
		}
	}

	m_firstMeshletIdxPerCluster.resize(meshToRender.m_clusters.size());
	m_meshIdx = instanceMeshRenderer->registerMesh(meshToRender, &m_firstMeshletIdxPerCluster);
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

uint32_t DrawManager::InstancedMeshRegistered::requestLoadingForMeshlet(uint32_t clusterIdx, uint32_t meshletIdx, const Wolf::ResourceNonOwner<Wolf::InstanceMeshRenderer>& instanceMeshRenderer, const Wolf::ResourceNonOwner<AssetManager>& assetManager) const
{
	Wolf::ResourceNonOwner<AssetMesh> meshAsset = assetManager->getMeshAsset(m_meshAssetId);
	bool meshletWasAlreadyLoaded = static_cast<bool>(meshAsset->getClusters()[clusterIdx]->m_meshlets[meshletIdx]->m_mesh);

	if (!meshletWasAlreadyLoaded)
	{
		meshAsset->loadMeshlet(clusterIdx, meshletIdx);
	}
	else
	{
		return -1;
	}

	instanceMeshRenderer->registerMeshletData(m_meshIdx, meshletIdx + m_firstMeshletIdxPerCluster[clusterIdx], meshAsset->getClusters()[clusterIdx]->m_meshlets[meshletIdx]->m_mesh.createNonOwnerResource<Wolf::MeshInterface>());
	return 0;
}

bool DrawManager::InstancedMeshRegistered::isSame(const Wolf::InstanceMeshRenderer::MeshToRender& otherMeshToRender) const
{
	if (Wolf::g_configuration->getUseMeshlets())
	{
		return m_lowLODMesh == otherMeshToRender.m_clusters[0].m_meshlets[0].m_mesh;
	}
	else
	{
		return m_lowLODMesh == otherMeshToRender.m_lods.back().m_mesh;
	}

}
