#include "AnimationHelper.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Debug.h"

void findMaxTimer(const Wolf::AnimationData::Bone* bone, float& maxTimer)
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

void computeBonesInfo(const Wolf::AnimationData::Bone* bone, glm::mat4 currentTransform, float time, const glm::mat4& modelTransform, std::vector<BoneInfoGPU>& outBonesInfoGPU, std::vector<BoneInfoCPU>& outBoneInfoCPU)
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
	outBonesInfoGPU[bone->idx].transform = currentTransform * bone->offsetMatrix;

	glm::vec3 offset = glm::inverse(bone->offsetMatrix) * glm::vec4(1.0f);
	outBoneInfoCPU[bone->idx].position = modelTransform * (outBonesInfoGPU[bone->idx].transform * glm::vec4(offset, 1.0f));

	for (const Wolf::AnimationData::Bone& childBone : bone->children)
	{
		computeBonesInfo(&childBone, currentTransform, time, modelTransform, outBonesInfoGPU, outBoneInfoCPU);
	}
}
