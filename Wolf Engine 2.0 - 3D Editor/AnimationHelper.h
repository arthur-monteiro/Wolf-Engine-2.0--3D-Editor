#pragma once

#include <DAEImporter.h>

struct BoneInfoGPU
{
	glm::mat4 transform;
};

struct BoneInfoCPU
{
	glm::vec3 position;
};

void findMaxTimer(const Wolf::AnimationData::Bone* bone, float& maxTimer);
void computeBonesInfo(const Wolf::AnimationData::Bone* bone, glm::mat4 currentTransform, float time, const glm::mat4& modelTransform, std::vector<BoneInfoGPU>& outBonesInfoGPU, std::vector<BoneInfoCPU>& outBoneInfoCPU);