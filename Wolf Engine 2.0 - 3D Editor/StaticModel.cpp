#include "StaticModel.h"

#include <AABB.h>
#include <Timer.h>
#include <Pipeline.h>
#include <ProfilerCommon.h>

#include "CommonLayouts.h"
#include "DrawManager.h"
#include "EditorConfiguration.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "TextureSetComponent.h"

StaticModel::StaticModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ResourceManager>& resourceManager,
	const std::function<void(ComponentInterface*)>& requestReloadCallback, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
: m_materialsGPUManager(materialsGPUManager), m_resourceManager(resourceManager), m_requestReloadCallback(requestReloadCallback), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
	m_defaultPipelineSet.reset(new Wolf::LazyInitSharedResource<Wolf::PipelineSet, StaticModel>([this](Wolf::ResourceUniqueOwner<Wolf::PipelineSet>& pipelineSet)
		{
			pipelineSet.reset(new Wolf::PipelineSet);

			Wolf::PipelineSet::PipelineInfo pipelineInfo;

			/* Pre Depth */
			pipelineInfo.shaderInfos.resize(1);
			pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/defaultPipeline/shader.vert";
			pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;

			// IA
			Wolf::Vertex3D::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);
			InstanceData::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 1);

			pipelineInfo.vertexInputBindingDescriptions.resize(2);
			Wolf::Vertex3D::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);
			InstanceData::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[1], 1);

			// Resources
			pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

			// Color Blend
			pipelineInfo.blendModes = {Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

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
			pipelineInfo.bindlessDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_BINDLESS;
			pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
			pipelineInfo.customMask = AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO;

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
			pipelineInfo.enableDepthWrite = false;
			pipelineInfo.depthCompareOp = VK_COMPARE_OP_EQUAL;

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);

			// Output Ids
			pipelineInfo.bindlessDescriptorSlot = -1;
			pipelineInfo.lightDescriptorSlot = -1;
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultPipeline/outputIds.frag";
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_OUTPUT_IDS);
		}));
}

void StaticModel::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_modelParams);
	::loadParams<SubMesh>(jsonReader, ID, m_editorParams, false);
	for (uint32_t i = 0; i < m_subMeshes.size(); ++i)
	{
		m_subMeshes[i].setReloadEntityCallback([this]() { reloadEntity(); });
		m_subMeshes[i].init(i, "Placeholder", m_getEntityFromLoadingPathCallback);
		m_subMeshes[i].loadParams(jsonReader, ID);
	}

	subscribeToAllSubMeshes();
}

void StaticModel::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	EditorModelInterface::updateBeforeFrame(globalTimer, inputHandler);

	if (m_isWaitingForMeshLoading)
	{
		if (m_resourceManager->isModelLoaded(m_meshResourceId))
		{
			m_isWaitingForMeshLoading = false;
			if (m_subMeshes.empty())
			{
				const std::vector<Wolf::MaterialsGPUManager::TextureSetInfo>& textureSetInfo = m_resourceManager->getModelData(m_meshResourceId)->textureSets;
				for (uint32_t subMeshIdx = 0; subMeshIdx < textureSetInfo.size(); ++subMeshIdx)
				{
					m_subMeshes.emplace_back();
					m_subMeshes.back().setReloadEntityCallback([this]() { reloadEntity(); });
					m_subMeshes.back().init(subMeshIdx, textureSetInfo[subMeshIdx].materialName, m_getEntityFromLoadingPathCallback);
					m_subMeshes.back().reset();
				}

				// Add a sub mesh if the mesh doesn't contain any material
				if (textureSetInfo.empty())
				{
					m_subMeshes.emplace_back();
					m_subMeshes.back().setReloadEntityCallback([this]() { reloadEntity(); });
					m_subMeshes.back().init(0, "No material", m_getEntityFromLoadingPathCallback);
					m_subMeshes.back().reset();
				}

				subscribeToAllSubMeshes();
				reloadEntity();
			}
		}
	}

	for (uint32_t subMeshIdx = 0; subMeshIdx < m_subMeshes.size(); ++subMeshIdx)
	{
		m_subMeshes[subMeshIdx].update(m_materialsGPUManager, m_resourceManager->getFirstTextureSetIdx(m_meshResourceId) + subMeshIdx);
	}
}

bool StaticModel::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
	PROFILE_FUNCTION

	if (!m_resourceManager->isModelLoaded(m_meshResourceId))
		return false;

	Wolf::RenderMeshList::MeshToRender meshToRenderInfo = { m_resourceManager->getModelData(m_meshResourceId)->mesh.createNonOwnerResource(), m_defaultPipelineSet->getResource().createConstNonOwnerResource() };

	uint32_t firstMaterialIdx = m_subMeshes[0].getMaterialIdx();
	if (firstMaterialIdx == SubMesh::DEFAULT_MATERIAL_IDX)
		firstMaterialIdx = m_resourceManager->getFirstMaterialIdx(m_meshResourceId);

	InstanceData instanceData{};
	instanceData.transform = m_transform;
	instanceData.firstMaterialIdx = firstMaterialIdx;
	instanceData.entityIdx = m_entity->getIdx();
	outList.push_back({ meshToRenderInfo, instanceData});

	return true;
}

bool StaticModel::getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::TLASInfo::InstanceInfo>& instanceInfos)
{
	PROFILE_FUNCTION

	if (!m_resourceManager->isModelLoaded(m_meshResourceId))
		return false;

	RayTracedWorldManager::TLASInfo::InstanceInfo instanceInfo { m_resourceManager->getBLAS(m_meshResourceId), m_transform };
	instanceInfos.push_back(instanceInfo);

	return true;
}

bool StaticModel::getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList)
{
	if (!m_resourceManager->isModelLoaded(m_meshResourceId))
		return false;

	for (Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& physicsShape : m_resourceManager->getModelData(m_meshResourceId)->physicsShapes)
	{
		outList.push_back({physicsShape.createNonOwnerResource(), m_transform });
	}

	return true;
}

void StaticModel::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
}

void StaticModel::activateParams()
{
	EditorModelInterface::activateParams();

	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void StaticModel::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	EditorModelInterface::addParamsToJSON(outJSON, tabCount);

	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

Wolf::AABB StaticModel::getAABB() const
{
	if (m_resourceManager->isModelLoaded(m_meshResourceId))
		return m_resourceManager->getModelData(m_meshResourceId)->mesh->getAABB() * m_transform;

	return Wolf::AABB();
}

Wolf::BoundingSphere StaticModel::getBoundingSphere() const
{
	if (m_resourceManager->isModelLoaded(m_meshResourceId))
		return m_resourceManager->getModelData(m_meshResourceId)->mesh->getBoundingSphere() * m_scaleParam + m_translationParam;

	return Wolf::BoundingSphere();
}

void StaticModel::subscribeToAllSubMeshes()
{
	for (uint32_t i = 0; i < m_subMeshes.size(); ++i)
	{
		m_subMeshes[i].subscribe(this, [this](Flags) { onSubMeshChanged(); });
	}
}

void StaticModel::requestModelLoading()
{
	if (!std::string(m_loadingPathParam).empty())
	{
		m_subMeshes.clear();

		m_meshResourceId = m_resourceManager->addModel(std::string(m_loadingPathParam));
		m_resourceManager->subscribeToResource(m_meshResourceId, this, [this](Flags) { notifySubscribers(); });
		m_isWaitingForMeshLoading = true;
		notifySubscribers();
	}
}

void StaticModel::reloadEntity()
{
	m_requestReloadCallback(this);
}

void StaticModel::onSubMeshChanged()
{
	// TODO: Check that material indices are contiguous (as we only provide first material idx)

	notifySubscribers();
}

StaticModel::SubMesh::SubMesh() : ParameterGroupInterface(TAB)
{
}

void StaticModel::SubMesh::update(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, uint32_t resourceTextureSetIdx)
{
	std::vector<uint32_t> delayedIndices;
	for (uint32_t indexOfTextureSetInMaterial : m_textureSetChangedIndices)
	{
		const TextureSet& textureSet = m_textureSets[indexOfTextureSetInMaterial];

		uint32_t textureSetGPUIdx = textureSet.getTextureSetIdx();
		if (textureSetGPUIdx == TextureSet::NO_TEXTURE_SET_IDX)
		{
			textureSetGPUIdx = resourceTextureSetIdx;
		}
		else if (textureSetGPUIdx == 0) // texture set not loaded yet
		{
			delayedIndices.push_back(indexOfTextureSetInMaterial);
			continue;
		}
		float strength = textureSet.getStrength();

		if (m_materialIdx == DEFAULT_MATERIAL_IDX)
		{
			Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
			materialInfo.textureSetInfos[indexOfTextureSetInMaterial].textureSetIdx = textureSetGPUIdx;
			materialInfo.textureSetInfos[indexOfTextureSetInMaterial].strength = strength;

			materialsGPUManager->lockMaterials();
			m_materialIdx = materialsGPUManager->getCurrentMaterialCount();
			materialsGPUManager->addNewMaterial(materialInfo);
			materialsGPUManager->unlockMaterials();

			// TODO: delay the rest
		}
		else
		{
			materialsGPUManager->changeTextureSetIdxBeforeFrame(m_materialIdx, indexOfTextureSetInMaterial, textureSetGPUIdx);
			materialsGPUManager->changeStrengthBeforeFrame(m_materialIdx, indexOfTextureSetInMaterial, strength);
		}
	}

	if (!m_textureSetChangedIndices.empty())
		notifySubscribers();

	m_textureSetChangedIndices.clear();
	m_textureSetChangedIndices.swap(delayedIndices);
}

void StaticModel::SubMesh::setReloadEntityCallback(const std::function<void()>& callback)
{
	m_reloadEntityCallback = callback;
}

void StaticModel::SubMesh::init(uint32_t idx, const std::string& defaultMaterialName, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
{
	m_subMeshIdx = idx;
	m_defaultMaterialName = defaultMaterialName;
	m_getEntityFromLoadingPathCallback = getEntityFromLoadingPathCallback;
}

void StaticModel::SubMesh::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void StaticModel::SubMesh::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void StaticModel::SubMesh::loadParams(Wolf::JSONReader& jsonReader, const std::string& objectId)
{
	std::vector<EditorParamInterface*> arrayItemParams;
	getAllParams(arrayItemParams);
	arrayItemParams.push_back(getNameParam());
	::loadParams<TextureSet>(jsonReader, objectId, arrayItemParams);
}

void StaticModel::SubMesh::reset()
{
	m_textureSets.clear();

	m_name = "Sub mesh " + std::to_string(m_subMeshIdx);
	m_textureSets.emplace_back();
	m_textureSets.back().setName("Default (" + m_defaultMaterialName + ")");
}

void StaticModel::SubMesh::onShadingModeChanged()
{
	if (!m_textureSets.empty())
		reset();
}

void StaticModel::SubMesh::onTextureSetChanged(uint32_t textureSetIdx)
{
	m_textureSetChangedIndices.push_back(textureSetIdx);
}

StaticModel::SubMesh::TextureSet::TextureSet() : ParameterGroupInterface(TAB)
{
	m_name = DEFAULT_NAME;
	m_strength = 1.0f;
}

void StaticModel::SubMesh::TextureSet::setDefaultName(const std::string& value)
{
	m_textureSetEntityParam.setNoEntitySelectedString(value);
}

void StaticModel::SubMesh::TextureSet::setGetEntityFromLoadingPathCallback(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
{
	m_getEntityFromLoadingPathCallback = getEntityFromLoadingPathCallback;
}

void StaticModel::SubMesh::TextureSet::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void StaticModel::SubMesh::TextureSet::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

bool StaticModel::SubMesh::TextureSet::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

uint32_t StaticModel::SubMesh::TextureSet::getTextureSetIdx() const
{
	if (m_textureSetEntity)
	{
		if (const Wolf::NullableResourceNonOwner<TextureSetComponent> textureSetComponent = (*m_textureSetEntity)->getComponent<TextureSetComponent>())
		{
			return textureSetComponent->getTextureSetIdx();
		}
		else
		{
			Wolf::Debug::sendError("Entity should contain a texture set component");
		}
	}

	return NO_TEXTURE_SET_IDX;
}

void StaticModel::SubMesh::TextureSet::onTextureSetEntityChanged()
{
	if (static_cast<std::string>(m_textureSetEntityParam).empty())
	{
		m_textureSetEntity.reset(nullptr);
	}
	else
	{
		m_textureSetEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_textureSetEntityParam)));
	}

	notifySubscribers();
}

void StaticModel::SubMesh::onTextureSetAdded()
{
	m_textureSets.back().setDefaultName("Default (" + m_defaultMaterialName + ")");
	m_textureSets.back().setGetEntityFromLoadingPathCallback(m_getEntityFromLoadingPathCallback);

	uint32_t idx = static_cast<uint32_t>(m_textureSets.size()) - 1;
	m_textureSets.back().subscribe(this, [this, idx](Flags) { onTextureSetChanged(idx); });

	if (m_reloadEntityCallback)
		m_reloadEntityCallback();
}