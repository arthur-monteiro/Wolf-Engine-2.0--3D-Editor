#include "ExternalSceneComponent.h"

#include <ProfilerCommon.h>

#include "CommonLayouts.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"
#include "ExternalSceneLoader.h"

ExternalSceneComponent::ExternalSceneComponent(const Wolf::ResourceNonOwner<AssetManager>& assetManager) : m_assetManager(assetManager)
{
    m_defaultPipelineSet.reset(new Wolf::LazyInitSharedResource<Wolf::PipelineSet, ExternalSceneComponent>([this](Wolf::ResourceUniqueOwner<Wolf::PipelineSet>& pipelineSet)
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
			pipelineInfo.depthBiasConstantFactor = 4.0f;
			pipelineInfo.depthBiasSlopeFactor = 2.5f;
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
			pipelineInfo.depthCompareOp = Wolf::CompareOp::LESS_OR_EQUAL;
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultPipeline/customRender.frag";
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_CUSTOM_RENDER);
		}));
}

void ExternalSceneComponent::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams(jsonReader, ID, m_editorParams);
}

void ExternalSceneComponent::activateParams()
{
    for (EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->activate();
    }
}

void ExternalSceneComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
    for (const EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->addToJSON(outJSON, tabCount, false);
    }
}

void ExternalSceneComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
    if (m_isWaitingForSceneLoading)
    {
        if (m_assetManager->isSceneLoaded(m_sceneAssetId))
        {
            m_isWaitingForSceneLoading = false;
        }
    }
}

bool ExternalSceneComponent::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
    PROFILE_FUNCTION

    if (!areAllMeshLoaded())
    	return false;

	const std::vector<ExternalSceneLoader::InstanceData>& instances = m_assetManager->getSceneInstances(m_sceneAssetId);
	const std::vector<AssetId> modelAssetsId = m_assetManager->getSceneModelAssetIds(m_sceneAssetId);
	// TODO: for each model: m_assetManager->subscribeToMesh(modelAssetId, this, [this](Flags) { notifySubscribers(); });

	uint32_t firstMaterialIdx = m_assetManager->getSceneFirstMaterialIdx(m_sceneAssetId);

    for (const ExternalSceneLoader::InstanceData& instance : instances)
    {
        AssetId modelAssetId = modelAssetsId[instance.m_meshIdx];

        Wolf::InstanceMeshRenderer::MeshToRender meshToRenderInfo = { m_assetManager->getModelMesh(modelAssetId).duplicateAs<Wolf::MeshInterface>(), m_defaultPipelineSet->getResource().createConstNonOwnerResource() };

        InstanceData instanceData{};
        instanceData.transform = m_transform * instance.m_transform;
        instanceData.firstMaterialIdx = firstMaterialIdx + instance.m_materialIdx;
        instanceData.entityIdx = m_entity->getIdx();
        outList.push_back({ meshToRenderInfo, instanceData});
    }

    return true;
}

bool ExternalSceneComponent::getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList)
{
    return true;
}

bool ExternalSceneComponent::getInstancesForRayTracedWorld(
	std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos)
{
	// TODO
	// if (!areAllMeshLoaded())
	// 	return false;
	//
	// const std::vector<ExternalSceneLoader::InstanceData>& instances = m_assetManager->getSceneInstances(m_sceneAssetId);
	// const std::vector<AssetId> modelAssetsId = m_assetManager->getSceneModelAssetIds(m_sceneAssetId);
	// // TODO: for each model: m_assetManager->subscribeToMesh(modelAssetId, this, [this](Flags) { notifySubscribers(); });
	//
	// uint32_t firstMaterialIdx = m_assetManager->getSceneFirstMaterialIdx(m_sceneAssetId);
	//
	// for (const ExternalSceneLoader::InstanceData& instance : instances)
	// {
	// 	AssetId modelAssetId = modelAssetsId[instance.m_meshIdx];
	//
	// 	RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo instanceInfo { m_assetManager->getBLAS(modelAssetId, 0, 0), m_transform, firstMaterialIdx,
	// 		m_assetManager->getModelMesh(modelAssetId) };
	//
	// 	instanceInfos.push_back(instanceInfo);
	// }

	return true;
}

Wolf::AABB ExternalSceneComponent::getAABB() const
{
    if (m_assetManager->isSceneLoaded(m_sceneAssetId))
        return m_assetManager->getSceneAABB(m_sceneAssetId) * m_transform;

    return Wolf::AABB();
}

Wolf::BoundingSphere ExternalSceneComponent::getBoundingSphere() const
{
    return Wolf::BoundingSphere(getAABB());
}

bool ExternalSceneComponent::areAllMeshLoaded()
{
	if (!m_assetManager->isSceneLoaded(m_sceneAssetId))
		return false;

	const std::vector<AssetId> modelAssetsId = m_assetManager->getSceneModelAssetIds(m_sceneAssetId);

	bool hasAMeshNotLoaded = false;
	for (AssetId modelAssetId : modelAssetsId)
	{
		if (!m_assetManager->isModelLoaded(modelAssetId))
		{
			hasAMeshNotLoaded = true;
		}
	}

	if (hasAMeshNotLoaded)
		return false;

	return true;
}

void ExternalSceneComponent::requestSceneLoading()
{
    std::string loadingPathStr = std::string(m_loadingPathParam);
    if (!loadingPathStr.empty())
    {
        if (std::string sanitizedLoadingPath = EditorConfiguration::sanitizeFilePath(loadingPathStr); sanitizedLoadingPath != loadingPathStr)
        {
            m_loadingPathParam = sanitizedLoadingPath;
            return;
        }

        m_sceneAssetId = m_assetManager->addExternalScene(loadingPathStr);
        m_isWaitingForSceneLoading = true;
        notifySubscribers();
    }
}
