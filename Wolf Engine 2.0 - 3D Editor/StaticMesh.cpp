#include "StaticMesh.h"

#include <AABB.h>
#include <Pipeline.h>
#include <ProfilerCommon.h>

#include "CommonLayouts.h"
#include "DrawManager.h"
#include "EditorParamsHelper.h"
#include "Entity.h"

StaticMesh::StaticMesh(const Wolf::ResourceNonOwner<AssetManager>& assetManager) : m_assetManager(assetManager)
{
	m_defaultPipelineSet.reset(new Wolf::LazyInitSharedResource<Wolf::PipelineSet, StaticMesh>([](Wolf::ResourceUniqueOwner<Wolf::PipelineSet>& pipelineSet)
		{
			pipelineSet.reset(new Wolf::PipelineSet);

			Wolf::PipelineSet::PipelineInfo pipelineInfo;

			/* Pre Depth */
			pipelineInfo.shaderInfos.resize(1);
			pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/defaultPipeline/shader.vert";
			pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;

			// IA
			Vertex3D::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

			pipelineInfo.vertexInputBindingDescriptions.resize(1);
			Vertex3D::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

			// Resources
			pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

			// Color Blend
			pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(Wolf::DynamicState::VIEWPORT);

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH);

			/* Shadow maps */
			pipelineInfo.dynamicStates.clear();
			pipelineInfo.depthBiasConstantFactor = -4.0f;
			pipelineInfo.depthBiasSlopeFactor = -2.5f;
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
			pipelineInfo.depthBiasConstantFactor = 0.0f;
			pipelineInfo.depthBiasSlopeFactor = 0.0f;

			/* Forward */
			pipelineInfo.shaderInfos.resize(2);
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultPipeline/shader.frag";
			pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

			// Resources
			pipelineInfo.materialsDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MATERIAL_MANAGER;
			pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
			pipelineInfo.customMask = AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO | AdditionalDescriptorSetsMaskBits::GLOBAL_IRRADIANCE_SHADOW_MASK_INFO;

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(Wolf::DynamicState::VIEWPORT);
			pipelineInfo.enableDepthWrite = false;
			pipelineInfo.depthCompareOp = Wolf::CompareOp::EQUAL;

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);

			// Output Ids
			pipelineInfo.materialsDescriptorSlot = -1;
			pipelineInfo.lightDescriptorSlot = -1;
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultPipeline/outputIds.frag";
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_OUTPUT_IDS);

			// Custom depth
			pipelineInfo.materialsDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MATERIAL_MANAGER;
			pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
			pipelineInfo.dynamicStates.clear();
			pipelineInfo.enableDepthWrite = true;
			pipelineInfo.depthCompareOp = Wolf::CompareOp::GREATER_OR_EQUAL;
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultPipeline/customRender.frag";
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_CUSTOM_RENDER);
		}));
}

void StaticMesh::loadParams(Wolf::JSONReader& jsonReader)
{
	EditorMeshInterface::loadParams(jsonReader, ID);
	::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_alwaysVisibleEditorParams, false);
}

void StaticMesh::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	EditorMeshInterface::updateBeforeFrame(globalTimer, inputHandler);

	if (m_isWaitingForMeshLoading)
	{
		if (m_assetManager->isMeshLoaded(m_meshAssetId))
		{
			onRayTracedWorldLODTypeChanged();

			m_isWaitingForMeshLoading = false;
		}
	}
}

bool StaticMesh::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
	PROFILE_FUNCTION

	if (m_meshAssetId == NO_ASSET)
		return true;

	if (!m_assetManager->isMeshLoaded(m_meshAssetId))
		return false;

	InstanceData instanceData{};
	instanceData.m_transform = m_transform;
	AssetId materialAssetId = m_materialAssetId != NO_ASSET ? m_materialAssetId : m_assetManager->getDefaultMeshMaterialAssetId(m_meshAssetId);
	instanceData.m_materialIdx = materialAssetId == NO_ASSET ? 0 : m_assetManager->getMaterialEditor(materialAssetId)->getMaterialGPUIdx();
	instanceData.m_entityIdx = m_entity->getIdx();
	outList.push_back({ m_meshAssetId, Wolf::NullableResourceNonOwner<Wolf::MeshInterface>(), m_defaultPipelineSet->getResource().createConstNonOwnerResource(), {}, instanceData});

	return true;
}

bool StaticMesh::getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos)
{
	PROFILE_FUNCTION

	if (!m_assetManager->isMeshLoaded(m_meshAssetId))
		return false;

	AssetId materialAssetId = m_assetManager->getDefaultMeshMaterialAssetId(m_meshAssetId);
	uint32_t materialGPUIdx = materialAssetId == NO_ASSET ? 0 : m_assetManager->getMaterialEditor(materialAssetId)->getMaterialGPUIdx();

	Wolf::ResourceNonOwner<AssetMesh> meshAsset = m_assetManager->getMeshAsset(m_meshAssetId);
	Wolf::NullableResourceNonOwner<Wolf::Mesh> mesh;
	if (mesh = meshAsset->getLOD(m_rayTracedWorldLOD, m_rayTracedWorldLODType).m_mesh; !mesh)
	{
		meshAsset->loadLOD(m_rayTracedWorldLOD, m_rayTracedWorldLODType);
		return false;
	}

	RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo instanceInfo { m_assetManager->getBLAS(m_meshAssetId, m_rayTracedWorldLOD, m_rayTracedWorldLODType), m_transform,
		materialGPUIdx, mesh};

	instanceInfos.push_back(instanceInfo);

	return true;
}

bool StaticMesh::getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList)
{
	if (!m_assetManager->isMeshLoaded(m_meshAssetId))
		return false;

	for (Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& physicsShape : m_assetManager->getPhysicsShapes(m_meshAssetId))
	{
		outList.push_back({physicsShape.createNonOwnerResource(), m_transform });
	}

	return true;
}

void StaticMesh::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
}

void StaticMesh::activateParams()
{
	EditorMeshInterface::activateParams();

	for (EditorParamInterface* editorParam : m_alwaysVisibleEditorParams)
	{
		editorParam->activate();
	}
}

void StaticMesh::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	EditorMeshInterface::addParamsToJSON(outJSON, tabCount);

	for (const EditorParamInterface* editorParam : m_alwaysVisibleEditorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void StaticMesh::setInfoFromParent(AssetId modelAssetId)
{
	m_meshAssetId = modelAssetId;

	m_assetManager->subscribeToMesh(m_meshAssetId, this, [this](Flags) { notifySubscribers(); });
	m_isWaitingForMeshLoading = true;
	notifySubscribers();
}

Wolf::AABB StaticMesh::getAABB() const
{
	if (m_assetManager->isMeshLoaded(m_meshAssetId))
		return m_assetManager->getMeshAsset(m_meshAssetId)->getBoundingBox() * m_transform;

	return Wolf::AABB();
}

Wolf::BoundingSphere StaticMesh::getBoundingSphere() const
{
	if (m_assetManager->isMeshLoaded(m_meshAssetId))
		return m_assetManager->getMeshAsset(m_meshAssetId)->getBoundingSphere() * m_transform;

	return Wolf::BoundingSphere();
}

AssetId StaticMesh::getMaterialAssetId() const
{
	return m_assetManager->getDefaultMeshMaterialAssetId(m_meshAssetId);
}

void StaticMesh::onMeshAssetChanged()
{
	if (static_cast<std::string>(m_meshAssetParam) == "")
		return;

	AssetId meshAssetId = m_assetManager->getAssetIdForPath(m_meshAssetParam);
	if (!m_assetManager->isMesh(meshAssetId))
	{
		Wolf::Debug::sendWarning("Asset is not a mesh");
		m_meshAssetParam = "";
	}

	m_meshAssetId = meshAssetId;
	notifySubscribers();
}

void StaticMesh::onMaterialAssetChanged()
{
	if (static_cast<std::string>(m_materialAssetParam) == "")
		return;

	AssetId materialAssetId = m_assetManager->getAssetIdForPath(m_materialAssetParam);
	if (!m_assetManager->isMaterial(materialAssetId))
	{
		Wolf::Debug::sendWarning("Asset is not a material");
		m_meshAssetParam = "";
	}

	m_materialAssetId = materialAssetId;
	notifySubscribers();
}

void StaticMesh::onRayTracedWorldLODTypeChanged()
{
	// TODO: fix this
	// if (m_assetManager->isMeshLoaded(m_meshAssetId))
	// {
	// 	uint32_t maxLOD;
	// 	if (m_rayTracedWorldLODType == 0) // Default
	// 	{
	// 		maxLOD = m_assetManager->getMeshDefaultSimplifiedMeshes(m_meshAssetId).size();
	// 	}
	// 	else if (m_rayTracedWorldLODType == 1) // Sloppy
	// 	{
	// 		maxLOD = m_assetManager->getMeshSloppySimplifiedMeshes(m_meshAssetId).size();
	// 	}
	// 	else
	// 	{
	// 		Wolf::Debug::sendCriticalError("Unhandled draw LOD type");
	// 		maxLOD = 0;
	// 	}
	//
	// 	m_rayTracedWorldLOD.setMax(maxLOD);
	// }
	// notifySubscribers();
}
