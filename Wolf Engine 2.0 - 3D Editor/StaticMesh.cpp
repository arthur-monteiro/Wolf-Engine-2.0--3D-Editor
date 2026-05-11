#include "StaticMesh.h"

#include <AABB.h>
#include <Timer.h>
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
	EditorModelInterface::loadParams(jsonReader, ID);
	::loadParams(jsonReader.getRoot()->getPropertyObject(ID), ID, m_alwaysVisibleEditorParams, false);
}

void StaticMesh::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	EditorModelInterface::updateBeforeFrame(globalTimer, inputHandler);

	if (m_isWaitingForMeshLoading)
	{
		if (m_assetManager->isMeshLoaded(m_modelAssetId))
		{
			onRayTracedWorldLODTypeChanged();

			m_isWaitingForMeshLoading = false;
		}
	}
}

bool StaticMesh::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
	PROFILE_FUNCTION

	if (m_modelAssetId == NO_ASSET)
		return true;

	if (!m_assetManager->isMeshLoaded(m_modelAssetId))
		return false;

	float radius = m_assetManager->getMesh(m_modelAssetId)->getBoundingSphere().getRadius();
	constexpr float quality = 1.0f;

	std::vector<Wolf::ResourceNonOwner<Wolf::Mesh>> defaultLODs = m_assetManager->getMeshDefaultSimplifiedMeshes(m_modelAssetId);

	Wolf::InstanceMeshRenderer::MeshToRender meshToRenderInfo = { m_defaultPipelineSet->getResource().createConstNonOwnerResource() };
	meshToRenderInfo.m_lods.emplace_back(m_assetManager->getMesh(m_modelAssetId).duplicateAs<Wolf::MeshInterface>(),
		defaultLODs.empty() ? 10'000.0f : Wolf::InstanceMeshRenderer::computeLODDistance(radius, m_assetManager->getMesh(m_modelAssetId)->getIndexCount(), quality));

	for (uint32_t lod = 0; lod < defaultLODs.size(); ++lod)
	{
		float lodDistance = lod == defaultLODs.size() - 1 ? 10'000.0f : Wolf::InstanceMeshRenderer::computeLODDistance(radius, defaultLODs[lod]->getIndexCount(), quality);
		meshToRenderInfo.m_lods.emplace_back(defaultLODs[lod].duplicateAs<Wolf::MeshInterface>(), lodDistance);
	}

	InstanceData instanceData{};
	instanceData.transform = m_transform;
	instanceData.firstMaterialIdx = m_assetManager->getMaterialIdx(m_modelAssetId);
	instanceData.entityIdx = m_entity->getIdx();
	outList.push_back({ meshToRenderInfo, instanceData});

	return true;
}

bool StaticMesh::getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos)
{
	PROFILE_FUNCTION

	if (!m_assetManager->isMeshLoaded(m_modelAssetId))
		return false;

	RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo instanceInfo { m_assetManager->getBLAS(m_modelAssetId, m_rayTracedWorldLOD, m_rayTracedWorldLODType), m_transform,
		m_assetManager->getMaterialIdx(m_modelAssetId), m_assetManager->getMesh(m_modelAssetId) };

	if (m_rayTracedWorldLOD > 0)
	{
		if (m_rayTracedWorldLODType == 0) // Default
		{
			instanceInfo.m_mesh = m_assetManager->getMeshDefaultSimplifiedMeshes(m_modelAssetId)[m_rayTracedWorldLOD - 1];
		}
		else if (m_rayTracedWorldLODType == 1) // Sloppy
		{
			instanceInfo.m_mesh = m_assetManager->getMeshSloppySimplifiedMeshes(m_modelAssetId)[m_rayTracedWorldLOD - 1];
		}
		else
		{
			Wolf::Debug::sendCriticalError("Unhandled LOD type");
		}
	}

	instanceInfos.push_back(instanceInfo);

	return true;
}

bool StaticMesh::getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList)
{
	if (!m_assetManager->isMeshLoaded(m_modelAssetId))
		return false;

	for (Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& physicsShape : m_assetManager->getPhysicsShapes(m_modelAssetId))
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
	EditorModelInterface::activateParams();

	for (EditorParamInterface* editorParam : m_alwaysVisibleEditorParams)
	{
		editorParam->activate();
	}
}

void StaticMesh::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	EditorModelInterface::addParamsToJSON(outJSON, tabCount);

	for (const EditorParamInterface* editorParam : m_alwaysVisibleEditorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void StaticMesh::setInfoFromParent(AssetId modelAssetId)
{
	m_modelAssetId = modelAssetId;

	m_assetManager->subscribeToMesh(m_modelAssetId, this, [this](Flags) { notifySubscribers(); });
	m_isWaitingForMeshLoading = true;
	notifySubscribers();
}

Wolf::AABB StaticMesh::getAABB() const
{
	if (m_assetManager->isMeshLoaded(m_modelAssetId))
		return m_assetManager->getMesh(m_modelAssetId)->getAABB() * m_transform;

	return Wolf::AABB();
}

Wolf::BoundingSphere StaticMesh::getBoundingSphere() const
{
	if (m_assetManager->isMeshLoaded(m_modelAssetId))
		return m_assetManager->getMesh(m_modelAssetId)->getBoundingSphere() * m_transform;

	return Wolf::BoundingSphere();
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

	m_modelAssetId = meshAssetId;
	notifySubscribers();
}

void StaticMesh::onRayTracedWorldLODTypeChanged()
{
	if (m_assetManager->isMeshLoaded(m_modelAssetId))
	{
		uint32_t maxLOD;
		if (m_rayTracedWorldLODType == 0) // Default
		{
			maxLOD = m_assetManager->getMeshDefaultSimplifiedMeshes(m_modelAssetId).size();
		}
		else if (m_rayTracedWorldLODType == 1) // Sloppy
		{
			maxLOD = m_assetManager->getMeshSloppySimplifiedMeshes(m_modelAssetId).size();
		}
		else
		{
			Wolf::Debug::sendCriticalError("Unhandled draw LOD type");
			maxLOD = 0;
		}

		m_rayTracedWorldLOD.setMax(maxLOD);
	}
	notifySubscribers();
}
