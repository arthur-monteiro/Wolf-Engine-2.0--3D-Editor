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

StaticModel::StaticModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<AssetManager>& assetManager,
	const std::function<void(ComponentInterface*)>& requestReloadCallback, const std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
: m_materialsGPUManager(materialsGPUManager), m_assetManager(assetManager), m_requestReloadCallback(requestReloadCallback), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
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

void StaticModel::loadParams(Wolf::JSONReader& jsonReader)
{
	EditorModelInterface::loadParams(jsonReader, ID);
	::loadParams(jsonReader, ID, m_dataSourceFileEditorParams);
	::loadParams<SubMesh>(jsonReader, ID, m_alwaysVisibleEditorParams, false);
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
		if (m_assetManager->isModelLoaded(m_modelAssetId))
		{
			if (m_subMeshes.empty())
			{
				const std::vector<Wolf::MaterialsGPUManager::TextureSetInfo>& textureSetInfo = m_assetManager->getModelTextureSetInfo(m_modelAssetId);
				for (uint32_t subMeshIdx = 0; subMeshIdx < textureSetInfo.size(); ++subMeshIdx)
				{
					m_subMeshes.emplace_back();
					m_subMeshes.back().setReloadEntityCallback([this]() { reloadEntity(); });
					m_subMeshes.back().init(subMeshIdx, textureSetInfo[subMeshIdx].name, m_getEntityFromLoadingPathCallback);
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

			onDrawLODTypeChanged();
			onRayTracedWorldLODTypeChanged();

			m_isWaitingForMeshLoading = false;
		}
	}

	if (m_assetManager->isModelLoaded(m_modelAssetId))
	{
		for (uint32_t subMeshIdx = 0; subMeshIdx < m_subMeshes.size(); ++subMeshIdx)
		{
			m_subMeshes[subMeshIdx].update(m_materialsGPUManager, m_assetManager->getFirstTextureSetIdx(m_modelAssetId) + subMeshIdx);
		}
	}
}

bool StaticModel::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
	PROFILE_FUNCTION

	if (!m_assetManager->isModelLoaded(m_modelAssetId))
		return false;

	Wolf::InstanceMeshRenderer::MeshToRender meshToRenderInfo = { m_assetManager->getModelMesh(m_modelAssetId).duplicateAs<Wolf::MeshInterface>(), m_defaultPipelineSet->getResource().createConstNonOwnerResource() };
	if (m_drawLOD > 0)
	{
		// TODO: repair this
		// if (m_drawLODType == 0) // Default
		// {
		// 	meshToRenderInfo.m_overrideIndexBuffer = m_resourceManager->getModelDefaultSimplifiedIndexBuffers(m_modelAssetId)[m_drawLOD - 1];
		// }
		// else if (m_drawLODType == 1) // Sloppy
		// {
		// 	meshToRenderInfo.m_overrideIndexBuffer = m_resourceManager->getModelSloppySimplifiedIndexBuffers(m_modelAssetId)[m_drawLOD - 1];
		// }
		// else
		// {
		// 	Wolf::Debug::sendCriticalError("Unhandled LOD type");
		// }
	}

	InstanceData instanceData{};
	instanceData.transform = m_transform;
	instanceData.firstMaterialIdx = m_assetManager->getFirstMaterialIdx(m_modelAssetId);
	instanceData.entityIdx = m_entity->getIdx();
	outList.push_back({ meshToRenderInfo, instanceData});

	return true;
}

bool StaticModel::getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos)
{
	PROFILE_FUNCTION

	if (!m_assetManager->isModelLoaded(m_modelAssetId))
		return false;

	uint32_t firstMaterialIdx = m_subMeshes[0].getMaterialIdx();
	if (firstMaterialIdx == SubMesh::DEFAULT_MATERIAL_IDX)
		firstMaterialIdx = m_assetManager->getFirstMaterialIdx(m_modelAssetId);

	RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo instanceInfo { m_assetManager->getBLAS(m_modelAssetId, m_rayTracedWorldLOD, m_rayTracedWorldLODType), m_transform, firstMaterialIdx,
		m_assetManager->getModelMesh(m_modelAssetId) };

	if (m_rayTracedWorldLOD > 0)
	{
		if (m_rayTracedWorldLODType == 0) // Default
		{
			instanceInfo.m_mesh = m_assetManager->getModelDefaultSimplifiedMeshes(m_modelAssetId)[m_rayTracedWorldLOD - 1];
		}
		else if (m_rayTracedWorldLODType == 1) // Sloppy
		{
			instanceInfo.m_mesh = m_assetManager->getModelSloppySimplifiedMeshes(m_modelAssetId)[m_rayTracedWorldLOD - 1];
		}
		else
		{
			Wolf::Debug::sendCriticalError("Unhandled LOD type");
		}
	}

	instanceInfos.push_back(instanceInfo);

	return true;
}

bool StaticModel::getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList)
{
	if (!m_assetManager->isModelLoaded(m_modelAssetId))
		return false;

	for (Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& physicsShape : m_assetManager->getPhysicsShapes(m_modelAssetId))
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

	for (EditorParamInterface* editorParam : m_alwaysVisibleEditorParams)
	{
		editorParam->activate();
	}

	if (m_dataSource == DataSource::FILE)
	{
		for (EditorParamInterface* editorParam : m_dataSourceFileEditorParams)
		{
			editorParam->activate();
		}
	}
}

void StaticModel::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	EditorModelInterface::addParamsToJSON(outJSON, tabCount);

	for (const EditorParamInterface* editorParam : m_alwaysVisibleEditorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}

	if (m_dataSource == DataSource::FILE)
	{
		for (const EditorParamInterface* editorParam : m_dataSourceFileEditorParams)
		{
			editorParam->addToJSON(outJSON, tabCount, false);
		}
	}
}

void StaticModel::setInfoFromParent(AssetId modelAssetId)
{
	m_dataSource = DataSource::PARENT;
	m_modelAssetId = modelAssetId;

	m_assetManager->subscribeToMesh(m_modelAssetId, this, [this](Flags) { notifySubscribers(); });
	m_isWaitingForMeshLoading = true;
	notifySubscribers();
}

Wolf::AABB StaticModel::getAABB() const
{
	if (m_assetManager->isModelLoaded(m_modelAssetId))
		return m_assetManager->getModelMesh(m_modelAssetId)->getAABB() * m_transform;

	return Wolf::AABB();
}

Wolf::BoundingSphere StaticModel::getBoundingSphere() const
{
	if (m_assetManager->isModelLoaded(m_modelAssetId))
		return m_assetManager->getModelMesh(m_modelAssetId)->getBoundingSphere() * m_transform;

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
	std::string loadingPathStr = std::string(m_loadingPathParam);
	if (!loadingPathStr.empty())
	{
		if (std::string sanitizedLoadingPath = EditorConfiguration::sanitizeFilePath(loadingPathStr); sanitizedLoadingPath != loadingPathStr)
		{
			m_loadingPathParam = sanitizedLoadingPath;
			return;
		}

		m_subMeshes.clear();

		m_modelAssetId = m_assetManager->addModel(loadingPathStr);
		m_assetManager->subscribeToMesh(m_modelAssetId, this, [this](Flags) { notifySubscribers(); });
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
		if (const Wolf::NullableResourceNonOwner<TextureSetComponent> textureSetComponent = m_textureSetEntity->getComponent<TextureSetComponent>())
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
		m_textureSetEntity = Wolf::NullableResourceNonOwner<Entity>();
	}
	else
	{
		m_textureSetEntity = m_getEntityFromLoadingPathCallback(m_textureSetEntityParam);
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

void StaticModel::onDrawLODTypeChanged()
{
	if (m_assetManager->isModelLoaded(m_modelAssetId))
	{
		uint32_t maxLOD;
		if (m_drawLODType == 0) // Default
		{
			maxLOD = m_assetManager->getModelDefaultSimplifiedMeshes(m_modelAssetId).size();
		}
		else if (m_drawLODType == 1) // Sloppy
		{
			maxLOD = m_assetManager->getModelSloppySimplifiedMeshes(m_modelAssetId).size();
		}
		else
		{
			Wolf::Debug::sendCriticalError("Unhandled draw LOD type");
			maxLOD = 0;
		}

		m_drawLOD.setMax(maxLOD);
	}
	notifySubscribers();
}

void StaticModel::onRayTracedWorldLODTypeChanged()
{
	if (m_assetManager->isModelLoaded(m_modelAssetId))
	{
		uint32_t maxLOD;
		if (m_rayTracedWorldLODType == 0) // Default
		{
			maxLOD = m_assetManager->getModelDefaultSimplifiedMeshes(m_modelAssetId).size();
		}
		else if (m_rayTracedWorldLODType == 1) // Sloppy
		{
			maxLOD = m_assetManager->getModelSloppySimplifiedMeshes(m_modelAssetId).size();
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
