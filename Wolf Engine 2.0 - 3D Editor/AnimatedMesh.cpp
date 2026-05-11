#include "AnimatedMesh.h"

#include <ProfilerCommon.h>
#include <glm/gtx/matrix_decompose.hpp>

#include "CommonLayouts.h"
#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "MaterialEditor.h"
#include "UpdateGPUBuffersPass.h"

AnimatedMesh::AnimatedMesh(const Wolf::ResourceNonOwner<AssetManager>& resourceManager, const std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback,
	const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback)
: m_assetManager(resourceManager), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback), m_updateGPUBuffersPass(renderingPipeline->getUpdateGPUBuffersPass()),
  m_requestReloadCallback(requestReloadCallback)
{
	m_defaultPipelineSet.reset(new Wolf::LazyInitSharedResource<Wolf::PipelineSet, AnimatedMesh>([](Wolf::ResourceUniqueOwner<Wolf::PipelineSet>& pipelineSet)
		{
			pipelineSet.reset(new Wolf::PipelineSet);

			Wolf::PipelineSet::PipelineInfo pipelineInfo;

			/* Pre Depth */
			pipelineInfo.shaderInfos.resize(1);
			pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/defaultAnimatedPipeline/shader.vert";
			pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;

			// IA
			SkeletonVertex::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

			pipelineInfo.vertexInputBindingDescriptions.resize(1);
			SkeletonVertex::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

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
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultAnimatedPipeline/shader.frag";
			pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

			pipelineInfo.shaderInfos[0].conditionBlocksToInclude.emplace_back("FORWARD");

			// Resources
			pipelineInfo.materialsDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MATERIAL_MANAGER;
			pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
			pipelineInfo.customMask = AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO | AdditionalDescriptorSetsMaskBits::GLOBAL_IRRADIANCE_SHADOW_MASK_INFO;

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(Wolf::DynamicState::VIEWPORT);

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
		}));

	m_descriptorSetLayoutGenerator.reset();
	m_descriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::VERTEX, 0);

	m_descriptorSetLayout.reset(new Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, AnimatedMesh>([this](Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& descriptorSetLayout)
		{
			descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));
		}));

	m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout->getResource()));
}

void AnimatedMesh::loadParams(Wolf::JSONReader& jsonReader)
{
	EditorModelInterface::loadParams(jsonReader, ID);
	::loadParams<Animation>(jsonReader.getRoot()->getPropertyObject(ID), ID, m_editorParams);
}

void AnimatedMesh::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	PROFILE_FUNCTION

	EditorModelInterface::updateBeforeFrame(globalTimer, inputHandler);

	if (m_waitingForMeshLoadingFrameCount > 0)
	{
		m_waitingForMeshLoadingFrameCount--;

		if (m_waitingForMeshLoadingFrameCount == 0 && m_assetManager->isMeshLoaded(m_meshAssetId))
		{
			Wolf::ResourceNonOwner<AnimationData> animationData = m_assetManager->getAnimationData(m_meshAssetId);

			m_boneCount = animationData->m_boneCount;
			m_bonesBuffer.reset(Wolf::Buffer::createBuffer(m_boneCount * sizeof(glm::mat4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
			m_bonesBuffer->setName("Animation bones (AnimatedModel::m_bonesBuffer)");

			m_bonesInfoGPU.resize(m_boneCount);
			m_bonesInfoCPU.resize(m_boneCount);
			m_updateMaxTimerRequested = true;

			Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
			descriptorSetGenerator.setBuffer(0, *m_bonesBuffer);

			m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

			// Bone names
			m_boneNamesAndIndices.clear();
			addBoneNamesAndIndices(animationData->m_rootBones.data());

			std::vector<std::string> boneNames(m_boneNamesAndIndices.size());
			for (uint32_t i = 0; i < boneNames.size(); ++i)
			{
				boneNames[i] = m_boneNamesAndIndices[i].first;
			}
			m_highlightBone.setOptions(boneNames);
		}
		else if (m_waitingForMeshLoadingFrameCount == 0)
		{
			m_waitingForMeshLoadingFrameCount = 1;
		}
	}
	else
	{
		// Animation update
		if (m_assetManager->isMeshLoaded(m_meshAssetId))
		{
			bool unused;
			Wolf::ResourceNonOwner<AnimationData> animationData = findAnimationData(unused);

			if (!m_forceTPoseParam)
			{
				float timer = static_cast<float>(globalTimer.getCurrentCachedMillisecondsDuration()) / 1000.0f;
				timer = fmod(timer, m_maxTimer);
				if (m_forceTimer.isEnabled())
				{
					timer = m_forceTimer;
				}
				computeBonesInfo(animationData->m_rootBones.data(), glm::mat4(1.0f), timer, m_transform, m_bonesInfoGPU, m_bonesInfoCPU);
			}
			else
			{
				for (BoneInfoGPU& boneInfo : m_bonesInfoGPU)
				{
					boneInfo.transform = glm::mat4(1.0f);
				}
			}
			m_updateGPUBuffersPass->addRequestBeforeFrame({ m_bonesInfoGPU.data(), static_cast<uint32_t>(m_bonesInfoGPU.size() * sizeof(BoneInfoGPU)), m_bonesBuffer.createNonOwnerResource(), 0 });
		}

		if (m_updateMaxTimerRequested)
		{
			updateMaxTimer();
		}
	}
}

bool AnimatedMesh::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
	PROFILE_FUNCTION

	if (m_waitingForMeshLoadingFrameCount > 0 || !m_assetManager->isMeshLoaded(m_meshAssetId))
		return false;

	if (m_hideModel == true)
		return true;

	Wolf::InstanceMeshRenderer::MeshToRender meshToRenderInfo = { m_defaultPipelineSet->getResource().createConstNonOwnerResource() };
	meshToRenderInfo.m_lods.emplace_back(m_assetManager->getMesh(m_meshAssetId).duplicateAs<Wolf::MeshInterface>(), 10'000.0f);

	Wolf::DescriptorSetBindInfo descriptorSetBindInfo(m_descriptorSet.createConstNonOwnerResource(), m_descriptorSetLayout->getResource().createConstNonOwnerResource(), 1);
	meshToRenderInfo.m_perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH].emplace_back(descriptorSetBindInfo);
	meshToRenderInfo.m_perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP].emplace_back(descriptorSetBindInfo);

	descriptorSetBindInfo.setBindingSlot(6);
	meshToRenderInfo.m_perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(descriptorSetBindInfo);

	InstanceData instanceData{};
	instanceData.transform = m_transform;
	instanceData.firstMaterialIdx = m_materialGPUIdx;
	instanceData.entityIdx = m_entity->getIdx();

	outList.push_back({meshToRenderInfo, instanceData});

	return true;
}

bool AnimatedMesh::getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList)
{
	// TODO
	return true;
}

void AnimatedMesh::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	if (m_showBonesParam)
	{
		if (m_assetManager->isMeshLoaded(m_meshAssetId))
		{
			Wolf::ResourceNonOwner<AnimationData> animationData = m_assetManager->getAnimationData(m_meshAssetId);
			addBonesToDebug(animationData->m_rootBones.data(), debugRenderingManager);
		}
	}
}

void AnimatedMesh::activateParams()
{
	EditorModelInterface::activateParams();

	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void AnimatedMesh::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	EditorModelInterface::addParamsToJSON(outJSON, tabCount);

	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

Wolf::AABB AnimatedMesh::getAABB() const
{
	if (m_assetManager->isMeshLoaded(m_meshAssetId))
		return m_assetManager->getMesh(m_meshAssetId)->getAABB() * m_transform;

	return Wolf::AABB();
}

Wolf::BoundingSphere AnimatedMesh::getBoundingSphere() const
{
	if (m_assetManager->isMeshLoaded(m_meshAssetId))
		return m_assetManager->getMesh(m_meshAssetId)->getBoundingSphere() * m_transform;

	return Wolf::BoundingSphere();
}

void AnimatedMesh::setInfoFromParent(AssetId assetId)
{
	m_meshAssetId = assetId;

	m_waitingForMeshLoadingFrameCount = 1;
	notifySubscribers();
}

void AnimatedMesh::getAnimationOptions(std::vector<std::string>& out)
{
	out = { "Default" };
	for (uint32_t i = 0; i < m_animationsParam.size(); ++i)
	{
		out.push_back(static_cast<std::string>(*m_animationsParam[i].getNameParam()));
	}
}

glm::vec3 AnimatedMesh::getBonePosition(uint32_t boneIdx) const
{
	return m_bonesInfoCPU[boneIdx].position;
}

void AnimatedMesh::setAnimation(uint32_t animationIdx)
{
	m_animationSelectParam = animationIdx;
}

void AnimatedMesh::addBonesToDebug(const AnimationData::Bone* bone, DebugRenderingManager& debugRenderingManager)
{
	if (m_bonesInfoGPU.empty())
		return;

	static constexpr float DEBUG_SPHERE_RADIUS = 0.05f;

	glm::vec3 offset = glm::inverse(bone->m_offsetMatrix) * glm::vec4(1.0f);
	glm::vec4 translation = m_transform * (m_bonesInfoGPU[bone->m_idx].transform * glm::vec4(offset, 1.0f));

	bool isHighlighted = m_boneNamesAndIndices[m_highlightBone].second == bone->m_idx;
	debugRenderingManager.addSphere(m_bonesInfoCPU[bone->m_idx].position, isHighlighted ? DEBUG_SPHERE_RADIUS * 1.5f : DEBUG_SPHERE_RADIUS, isHighlighted ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(1.0f));

	for (const AnimationData::Bone& childBone : bone->m_children)
	{
		addBonesToDebug(&childBone, debugRenderingManager);
	}
}

void AnimatedMesh::addBoneNamesAndIndices(const AnimationData::Bone* bone)
{
	m_boneNamesAndIndices.emplace_back(bone->m_name, bone->m_idx);

	for (const AnimationData::Bone& childBone : bone->m_children)
	{
		addBoneNamesAndIndices(&childBone);
	}
}

Wolf::NullableResourceNonOwner<AnimationData> AnimatedMesh::findAnimationData(bool& success)
{
	if (!m_assetManager->isMeshLoaded(m_meshAssetId))
	{
		success = false;
		return Wolf::NullableResourceNonOwner<AnimationData>();
	}

	success = true; // will be set to false if we encounter an error

	uint32_t animationIdx = m_animationSelectParam;

	Wolf::ResourceNonOwner<AnimationData> animationData = m_assetManager->getAnimationData(m_meshAssetId);
	if (animationIdx > 0)
	{
		uint32_t animationResourceId = m_animationsParam[animationIdx - 1 /* first one is default */].getAssetId();
		if (m_assetManager->isMeshLoaded(animationResourceId))
		{
			animationData = m_assetManager->getAnimationData(animationResourceId);
		}
		else
		{
			success = false;
		}
	}

	return animationData;
}

void AnimatedMesh::updateMaxTimer()
{
	m_maxTimer = 0.0f;

	bool success;
	Wolf::ResourceNonOwner<AnimationData> animationData = findAnimationData(success);

	findMaxTimer(animationData->m_rootBones.data(), m_maxTimer);
	m_forceTimer.setMax(m_maxTimer);

	m_requestReloadCallback(this);

	if (success)
	{
		m_updateMaxTimerRequested = false;
	}
}

void AnimatedMesh::onMeshChanged()
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
	m_waitingForMeshLoadingFrameCount = 1;
	notifySubscribers();
}

void AnimatedMesh::onMaterialAssetChanged()
{
	if (static_cast<std::string>(m_materialAsset) == "")
		return;

	m_materialGPUIdx = m_assetManager->getMaterialEditor(m_assetManager->getAssetIdForPath(m_materialAsset))->getMaterialGPUIdx();
	notifySubscribers();
}

AnimatedMesh::Animation::Animation() : ParameterGroupInterface(TAB, "Animation")
{
	m_name = DEFAULT_NAME;
}

void AnimatedMesh::Animation::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

void AnimatedMesh::Animation::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

bool AnimatedMesh::Animation::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

void AnimatedMesh::Animation::onAnimationParamChanged()
{
	if (static_cast<std::string>(m_animationAssetParam) == "")
		return;

	AssetId meshAssetId = m_assetManager->getAssetIdForPath(m_animationAssetParam);
	if (!m_assetManager->isMesh(meshAssetId))
	{
		Wolf::Debug::sendWarning("Asset is not a mesh");
		m_animationAssetParam = "";
	}

	m_assetId = meshAssetId;
	notifySubscribers();
}

void AnimatedMesh::onAnimationAdded()
{
	m_animationsParam.back().setAssetManager(m_assetManager);
	m_animationsParam.back().subscribe(this, [this](Flags) { updateAnimationsOptions(); });

	updateAnimationsOptions();
}

void AnimatedMesh::updateAnimationsOptions()
{
	std::vector<std::string> options;
	getAnimationOptions(options);
	m_animationSelectParam.setOptions(options);

	m_requestReloadCallback(this);

	notifySubscribers();
}