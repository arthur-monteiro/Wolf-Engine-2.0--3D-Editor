#include "AnimationHelper.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Debug.h"

void findMaxTimer(const AnimationData::Bone* bone, float& maxTimer)
{
	if (!bone->m_poses.empty() && bone->m_poses.back().m_time > maxTimer)
	{
		maxTimer = bone->m_poses.back().m_time;
	}

	for (const AnimationData::Bone& childBone : bone->m_children)
	{
		findMaxTimer(&childBone, maxTimer);
	}
}

void computeBonesInfo(const AnimationData::Bone* bone, glm::mat4 currentTransform, float time, const glm::mat4& modelTransform, std::vector<BoneInfoGPU>& outBonesInfoGPU, std::vector<BoneInfoCPU>& outBoneInfoCPU)
{
	glm::mat4 poseTransform(1.0f);
	if (!bone->m_poses.empty())
	{
		int32_t poseIdx;
		float lerpValue = 0.0f;
		for (poseIdx = 1; poseIdx < static_cast<int32_t>(bone->m_poses.size()); ++poseIdx)
		{
			if (time < bone->m_poses[poseIdx].m_time)
			{
				lerpValue = (time - bone->m_poses[poseIdx - 1].m_time) / (bone->m_poses[poseIdx].m_time - bone->m_poses[poseIdx - 1].m_time);
				break;
			}
		}
		if (poseIdx >= static_cast<int32_t>(bone->m_poses.size()))
		{
			poseIdx = 1;
			lerpValue = 0.0f;
		}
		if (lerpValue < 0.0f || lerpValue > 1.0f)
		{
			Wolf::Debug::sendError("Wrong lerp value");
		}


		glm::vec3 translation = glm::mix(bone->m_poses[poseIdx - 1].m_translation, bone->m_poses[poseIdx].m_translation, lerpValue);
		glm::quat orientation = glm::slerp(bone->m_poses[poseIdx - 1].m_orientation, bone->m_poses[poseIdx].m_orientation, lerpValue);
		glm::vec3 scale = glm::mix(bone->m_poses[poseIdx - 1].m_scale, bone->m_poses[poseIdx].m_scale, lerpValue);

		poseTransform = glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(orientation) * glm::scale(glm::mat4(1.0f), scale);
	}
	currentTransform = currentTransform * poseTransform;
	outBonesInfoGPU[bone->m_idx].transform = currentTransform * bone->m_offsetMatrix;

	glm::vec3 offset = glm::inverse(bone->m_offsetMatrix) * glm::vec4(1.0f);
	outBoneInfoCPU[bone->m_idx].position = modelTransform * (outBonesInfoGPU[bone->m_idx].transform * glm::vec4(offset, 1.0f));

	for (const AnimationData::Bone& childBone : bone->m_children)
	{
		computeBonesInfo(&childBone, currentTransform, time, modelTransform, outBonesInfoGPU, outBoneInfoCPU);
	}
}
