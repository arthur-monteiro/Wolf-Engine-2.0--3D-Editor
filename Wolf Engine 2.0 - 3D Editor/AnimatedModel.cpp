#include "AnimatedModel.h"

#include <ProfilerCommon.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "CommonLayouts.h"
#include "DAEImporter.h"
#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "MaterialComponent.h"
#include "TextureSetComponent.h"
#include "UpdateGPUBuffersPass.h"

AnimatedModel::AnimatedModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ResourceManager>& resourceManager, 
                             const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
							 const std::function<void(ComponentInterface*)>& requestReloadCallback)
: m_materialsGPUManager(materialsGPUManager), m_resourceManager(resourceManager), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback), m_updateGPUBuffersPass(renderingPipeline->getUpdateGPUBuffersPass()),
  m_requestReloadCallback(requestReloadCallback)
{
	m_defaultPipelineSet.reset(new Wolf::LazyInitSharedResource<Wolf::PipelineSet, AnimatedModel>([this](Wolf::ResourceUniqueOwner<Wolf::PipelineSet>& pipelineSet)
		{
			pipelineSet.reset(new Wolf::PipelineSet);

			Wolf::PipelineSet::PipelineInfo pipelineInfo;

			/* Pre Depth */
			pipelineInfo.shaderInfos.resize(1);
			pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/defaultAnimatedPipeline/shader.vert";
			pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;

			// IA
			Wolf::SkeletonVertex::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);
			InstanceData::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 1);

			pipelineInfo.vertexInputBindingDescriptions.resize(2);
			Wolf::SkeletonVertex::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);
			InstanceData::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[1], 1);

			// Resources
			pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

			// Color Blend
			pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

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
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultAnimatedPipeline/shader.frag";
			pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

			pipelineInfo.shaderInfos[0].conditionBlocksToInclude.emplace_back("FORWARD");

			// Resources
			pipelineInfo.bindlessDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_BINDLESS;
			pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
			pipelineInfo.customMask = AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO;

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
		}));
}

void AnimatedModel::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_modelParams);
	::loadParams<Animation>(jsonReader, ID, m_editorParams);
}

void AnimatedModel::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
	PROFILE_FUNCTION

	EditorModelInterface::updateBeforeFrame(globalTimer);

	if (m_waitingForMeshLoadingFrameCount > 0)
	{
		m_waitingForMeshLoadingFrameCount--;

		if (m_waitingForMeshLoadingFrameCount == 0 && m_resourceManager->isModelLoaded(m_meshResourceId))
		{
			std::unique_ptr<Wolf::AnimationData>& animationData = m_resourceManager->getModelData(m_meshResourceId)->animationData;

			m_boneCount = animationData->boneCount;
			m_bonesBuffer.reset(Wolf::Buffer::createBuffer(m_boneCount * sizeof(glm::mat4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

			m_bonesInfoGPU.resize(m_boneCount);
			m_bonesInfoCPU.resize(m_boneCount);
			m_updateMaxTimerRequested = true;

			if (!m_descriptorSetLayout)
			{
				m_descriptorSetLayoutGenerator.reset();
				m_descriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::VERTEX, 0);
				m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts()));

				m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
			}

			Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());
			descriptorSetGenerator.setBuffer(0, *m_bonesBuffer);

			m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

			// Bone names
			m_boneNamesAndIndices.clear();
			addBoneNamesAndIndices(animationData->rootBones.data());

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
		if (m_resourceManager->isModelLoaded(m_meshResourceId))
		{
			bool unused;
			Wolf::AnimationData* animationData = findAnimationData(unused);

			if (!m_forceTPoseParam)
			{
				float timer = static_cast<float>(globalTimer.getCurrentCachedMillisecondsDuration()) / 1000.0f;
				timer = fmod(timer, m_maxTimer);
				if (m_forceTimer.isEnabled())
				{
					timer = m_forceTimer;
				}
				computeBonesInfo(animationData->rootBones.data(), glm::mat4(1.0f), timer);
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

		if (m_textureSetIdxChanged)
		{
			uint32_t textureSetGPUIdx = getTextureSetIdx();
			if (textureSetGPUIdx == 0) // texture set not loaded yet
			{
				return;
			}

			if (m_materialIdx == MaterialComponent::DEFAULT_MATERIAL_IDX)
			{
				Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
				materialInfo.textureSetInfos[0].textureSetIdx = textureSetGPUIdx;
				materialInfo.textureSetInfos[0].strength = 1.0f;

				m_materialIdx = m_materialsGPUManager->getCurrentMaterialCount();
				m_materialsGPUManager->addNewMaterial(materialInfo);
			}
			else
			{
				m_materialsGPUManager->changeTextureSetIdxBeforeFrame(m_materialIdx, 0, textureSetGPUIdx);
			}

			m_textureSetIdxChanged = false;
			notifySubscribers();
		}

		if (m_updateMaxTimerRequested)
		{
			updateMaxTimer();
		}
	}
}

bool AnimatedModel::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
	PROFILE_FUNCTION

	if (m_waitingForMeshLoadingFrameCount > 0 || !m_resourceManager->isModelLoaded(m_meshResourceId))
		return false;

	if (m_hideModel == true)
		return true;

	Wolf::RenderMeshList::MeshToRender meshToRenderInfo = { m_resourceManager->getModelData(m_meshResourceId)->mesh.createNonOwnerResource(), m_defaultPipelineSet->getResource().createConstNonOwnerResource() };

	Wolf::DescriptorSetBindInfo descriptorSetBindInfo(m_descriptorSet.createConstNonOwnerResource(), m_descriptorSetLayout.createConstNonOwnerResource(), 1);
	meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH].emplace_back(descriptorSetBindInfo);
	meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP].emplace_back(descriptorSetBindInfo);

	descriptorSetBindInfo.setBindingSlot(5);
	meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].emplace_back(descriptorSetBindInfo);

	outList.push_back({ meshToRenderInfo, { m_transform, m_materialIdx } });

	return true;
}

bool AnimatedModel::getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList)
{
	// TODO
	return true;
}

void AnimatedModel::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
	if (m_showBonesParam)
	{
		if (m_resourceManager->isModelLoaded(m_meshResourceId))
		{
			std::unique_ptr<Wolf::AnimationData>& animationData = m_resourceManager->getModelData(m_meshResourceId)->animationData;
			addBonesToDebug(animationData->rootBones.data(), debugRenderingManager);
		}
	}
}

void AnimatedModel::activateParams()
{
	EditorModelInterface::activateParams();

	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void AnimatedModel::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	EditorModelInterface::addParamsToJSON(outJSON, tabCount);

	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

Wolf::AABB AnimatedModel::getAABB() const
{
	if (m_resourceManager->isModelLoaded(m_meshResourceId))
		return m_resourceManager->getModelData(m_meshResourceId)->mesh->getAABB() * m_transform;

	return Wolf::AABB();
}

Wolf::BoundingSphere AnimatedModel::getBoundingSphere() const
{
	if (m_resourceManager->isModelLoaded(m_meshResourceId))
		return m_resourceManager->getModelData(m_meshResourceId)->mesh->getBoundingSphere() * m_scaleParam + m_translationParam;

	return Wolf::BoundingSphere();
}

void AnimatedModel::getAnimationOptions(std::vector<std::string>& out)
{
	out = { "Default" };
	for (uint32_t i = 0; i < m_animationsParam.size(); ++i)
	{
		out.push_back(static_cast<std::string>(*m_animationsParam[i].getNameParam()));
	}
}

glm::vec3 AnimatedModel::getBonePosition(uint32_t boneIdx) const
{
	return m_bonesInfoCPU[boneIdx].position;
}

void AnimatedModel::setAnimation(uint32_t animationIdx)
{
	m_animationSelectParam = animationIdx;
}

void AnimatedModel::addBonesToDebug(const Wolf::AnimationData::Bone* bone, DebugRenderingManager& debugRenderingManager)
{
	if (m_bonesInfoGPU.empty())
		return;

	static constexpr float DEBUG_SPHERE_RADIUS = 0.05f;

	glm::vec3 offset = glm::inverse(bone->offsetMatrix) * glm::vec4(1.0f);
	glm::vec4 translation = m_transform * (m_bonesInfoGPU[bone->idx].transform * glm::vec4(offset, 1.0f));

	bool isHighlighted = m_boneNamesAndIndices[m_highlightBone].second == bone->idx;
	debugRenderingManager.addSphere(m_bonesInfoCPU[bone->idx].position, isHighlighted ? DEBUG_SPHERE_RADIUS * 1.5f : DEBUG_SPHERE_RADIUS, isHighlighted ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(1.0f));

	for (const Wolf::AnimationData::Bone& childBone : bone->children)
	{
		addBonesToDebug(&childBone, debugRenderingManager);
	}
}

void AnimatedModel::addBoneNamesAndIndices(const Wolf::AnimationData::Bone* bone)
{
	m_boneNamesAndIndices.emplace_back(bone->name, bone->idx);

	for (const Wolf::AnimationData::Bone& childBone : bone->children)
	{
		addBoneNamesAndIndices(&childBone);
	}
}

void AnimatedModel::findMaxTimer(const Wolf::AnimationData::Bone* bone, float& maxTimer)
{
	if (!bone->poses.empty() && bone->poses.back().time > maxTimer)
	{
		maxTimer = bone->poses.back().time;
	}

	for (const Wolf::AnimationData::Bone& childBone : bone->children)
	{
		findMaxTimer(&childBone, maxTimer);
	}
}

Wolf::AnimationData* AnimatedModel::findAnimationData(bool& success)
{
	if (!m_resourceManager->isModelLoaded(m_meshResourceId))
	{
		success = false;
		return nullptr;
	}

	success = true; // will be set to false if we encounter an error

	uint32_t animationIdx = m_animationSelectParam;

	Wolf::AnimationData* animationData = m_resourceManager->getModelData(m_meshResourceId)->animationData.get();
	if (animationIdx > 0)
	{
		uint32_t animationResourceId = m_animationsParam[animationIdx - 1 /* first one is default */].getResourceId();
		if (m_resourceManager->isModelLoaded(animationResourceId))
		{
			animationData = m_resourceManager->getModelData(animationResourceId)->animationData.get();
		}
		else
		{
			success = false;
		}
	}

	return animationData;
}

void AnimatedModel::updateMaxTimer()
{
	m_maxTimer = 0.0f;

	bool success;
	Wolf::AnimationData* animationData = findAnimationData(success);

	findMaxTimer(animationData->rootBones.data(), m_maxTimer);
	m_forceTimer.setMax(m_maxTimer);

	m_requestReloadCallback(this);

	if (success)
	{
		m_updateMaxTimerRequested = false;
	}
}

void AnimatedModel::requestModelLoading()
{
	if (!std::string(m_loadingPathParam).empty())
	{
		m_meshResourceId = m_resourceManager->addModel(std::string(m_loadingPathParam));
		m_waitingForMeshLoadingFrameCount = 2; // wait a bit to ensure bone matrices upload is finished
		notifySubscribers();
	}
}

void AnimatedModel::onTextureSetEntityChanged()
{
	if (static_cast<std::string>(m_textureSetEntityParam).empty())
	{
		m_textureSetEntity.reset(nullptr);
	}
	else
	{
		m_textureSetEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_getEntityFromLoadingPathCallback(m_textureSetEntityParam)));
	}

	if (m_textureSetEntity)
	{
		m_textureSetIdxChanged = true;
	}
}

uint32_t AnimatedModel::getTextureSetIdx() const
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

	return 0;
}

AnimatedModel::Animation::Animation() : ParameterGroupInterface(TAB)
{
	m_name = DEFAULT_NAME;
}

void AnimatedModel::Animation::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

void AnimatedModel::Animation::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

bool AnimatedModel::Animation::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

void AnimatedModel::Animation::onFileParamChanged()
{
	if (!std::string(m_fileParam).empty())
	{
		m_resourceId = (*m_resourceManager)->addModel(std::string(m_fileParam));
		notifySubscribers();
	}
}

void AnimatedModel::onAnimationAdded()
{
	m_animationsParam.back().setResourceManager(m_resourceManager);
	m_animationsParam.back().subscribe(this, [this]() { updateAnimationsOptions(); });

	updateAnimationsOptions();
}

void AnimatedModel::updateAnimationsOptions()
{
	std::vector<std::string> options;
	getAnimationOptions(options);
	m_animationSelectParam.setOptions(options);

	m_requestReloadCallback(this);

	notifySubscribers();
}

void AnimatedModel::computeBonesInfo(const Wolf::AnimationData::Bone* bone, glm::mat4 currentTransform, float time)
{
	glm::mat4 poseTransform(1.0f);
	if (!bone->poses.empty())
	{
		int32_t poseIdx;
		float lerpValue = 0.0f;
		for (poseIdx = 1; poseIdx < static_cast<int32_t>(bone->poses.size()); ++poseIdx)
		{
			if (time < bone->poses[poseIdx].time)
			{
				lerpValue = (time - bone->poses[poseIdx - 1].time) / (bone->poses[poseIdx].time - bone->poses[poseIdx - 1].time);
				break;
			}
		}
		if (poseIdx >= static_cast<int32_t>(bone->poses.size()))
		{
			poseIdx = 1;
			lerpValue = 0.0f;
		}
		if (lerpValue < 0.0f || lerpValue > 1.0f)
		{
			Wolf::Debug::sendError("Wrong lerp value");
		}


		glm::vec3 translation = glm::mix(bone->poses[poseIdx - 1].translation, bone->poses[poseIdx].translation, lerpValue);
		glm::quat orientation = glm::slerp(bone->poses[poseIdx - 1].orientation, bone->poses[poseIdx].orientation, lerpValue);
		glm::vec3 scale = glm::mix(bone->poses[poseIdx - 1].scale, bone->poses[poseIdx].scale, lerpValue);

		poseTransform = glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(orientation) * glm::scale(glm::mat4(1.0f), scale);
	}
	currentTransform = currentTransform * poseTransform;
	m_bonesInfoGPU[bone->idx].transform = currentTransform * bone->offsetMatrix;

	glm::vec3 offset = glm::inverse(bone->offsetMatrix) * glm::vec4(1.0f);
	m_bonesInfoCPU[bone->idx].position = m_transform * (m_bonesInfoGPU[bone->idx].transform * glm::vec4(offset, 1.0f));

	for (const Wolf::AnimationData::Bone& childBone : bone->children)
	{
		computeBonesInfo(&childBone, currentTransform, time);
	}
}
