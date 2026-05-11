#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "VertexInputs.h"

struct SkeletonVertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 texCoords;

	glm::ivec4 bonesIds;
	glm::vec4 bonesWeights;

	static void getBindingDescription(Wolf::VertexInputBindingDescription& bindingDescription, uint32_t binding)
	{
		bindingDescription.binding = binding;
		bindingDescription.stride = sizeof(SkeletonVertex);
		bindingDescription.inputRate = Wolf::VertexInputRate::VERTEX;
	}

	static void getAttributeDescriptions(std::vector<Wolf::VertexInputAttributeDescription>& attributeDescriptions, uint32_t binding)
	{
		const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
		attributeDescriptions.resize(attributeDescriptionCountBefore + 6);

		attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 0].location = 0;
		attributeDescriptions[attributeDescriptionCountBefore + 0].format = Wolf::Format::R32G32B32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(SkeletonVertex, pos);

		attributeDescriptions[attributeDescriptionCountBefore + 1].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 1].location = 1;
		attributeDescriptions[attributeDescriptionCountBefore + 1].format = Wolf::Format::R32G32B32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 1].offset = offsetof(SkeletonVertex, normal);

		attributeDescriptions[attributeDescriptionCountBefore + 2].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 2].location = 2;
		attributeDescriptions[attributeDescriptionCountBefore + 2].format = Wolf::Format::R32G32B32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 2].offset = offsetof(SkeletonVertex, tangent);

		attributeDescriptions[attributeDescriptionCountBefore + 3].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 3].location = 3;
		attributeDescriptions[attributeDescriptionCountBefore + 3].format = Wolf::Format::R32G32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 3].offset = offsetof(SkeletonVertex, texCoords);

		attributeDescriptions[attributeDescriptionCountBefore + 4].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 4].location = 4;
		attributeDescriptions[attributeDescriptionCountBefore + 4].format = Wolf::Format::R32G32B32A32_SINT;
		attributeDescriptions[attributeDescriptionCountBefore + 4].offset = offsetof(SkeletonVertex, bonesIds);

		attributeDescriptions[attributeDescriptionCountBefore + 5].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 5].location = 5;
		attributeDescriptions[attributeDescriptionCountBefore + 5].format = Wolf::Format::R32G32B32A32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 5].offset = offsetof(SkeletonVertex, bonesWeights);
	}

	bool operator==(const SkeletonVertex& other) const
	{
		return pos == other.pos && normal == other.normal && texCoords == other.texCoords && tangent == other.tangent;
	}
};

namespace std
{
	template<> struct hash<SkeletonVertex>
	{
		size_t operator()(SkeletonVertex const& vertex) const
		{
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoords) << 1);
		}
	};
}

struct AnimationData
{
	struct Bone
	{
		uint32_t m_idx;
		std::string m_name;
		glm::mat4 m_offsetMatrix;

		struct Pose
		{
			float m_time;
			glm::vec3 m_translation;
			glm::quat m_orientation;
			glm::vec3 m_scale;
		};
		std::vector<Pose> m_poses;

		std::vector<Bone> m_children;
	};
	std::vector<Bone> m_rootBones;

	uint32_t m_boneCount;
};