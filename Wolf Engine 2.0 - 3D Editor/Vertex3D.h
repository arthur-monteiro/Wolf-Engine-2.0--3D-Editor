#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <VertexInputs.h>

struct Vertex3D
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 texCoord;

	static void getBindingDescription(Wolf::VertexInputBindingDescription& bindingDescription, uint32_t binding)
	{
		bindingDescription.binding = binding;
		bindingDescription.stride = sizeof(Vertex3D);
		bindingDescription.inputRate = Wolf::VertexInputRate::VERTEX;
	}

	static void getAttributeDescriptions(std::vector<Wolf::VertexInputAttributeDescription>& attributeDescriptions, uint32_t binding)
	{
		const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
		attributeDescriptions.resize(attributeDescriptionCountBefore + 4);

		attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 0].location = 0;
		attributeDescriptions[attributeDescriptionCountBefore + 0].format = Wolf::Format::R32G32B32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(Vertex3D, pos);

		attributeDescriptions[attributeDescriptionCountBefore + 1].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 1].location = 1;
		attributeDescriptions[attributeDescriptionCountBefore + 1].format = Wolf::Format::R32G32B32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 1].offset = offsetof(Vertex3D, normal);

		attributeDescriptions[attributeDescriptionCountBefore + 2].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 2].location = 2;
		attributeDescriptions[attributeDescriptionCountBefore + 2].format = Wolf::Format::R32G32B32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 2].offset = offsetof(Vertex3D, tangent);

		attributeDescriptions[attributeDescriptionCountBefore + 3].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 3].location = 3;
		attributeDescriptions[attributeDescriptionCountBefore + 3].format = Wolf::Format::R32G32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 3].offset = offsetof(Vertex3D, texCoord);
	}

	bool operator==(const Vertex3D& other) const
	{
		return pos == other.pos && normal == other.normal && texCoord == other.texCoord && tangent == other.tangent;
	}
};

inline void computeNormals(std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices)
{
	for (Vertex3D& vertex : vertices)
	{
		vertex.normal = glm::vec3(0.0f);
	}

	for (size_t i = 0; i < indices.size(); i += 3)
	{
		const uint32_t i0 = indices[i];
		const uint32_t i1 = indices[i + 1];
		const uint32_t i2 = indices[i + 2];

		const glm::vec3& p0 = vertices[i0].pos;
		const glm::vec3& p1 = vertices[i1].pos;
		const glm::vec3& p2 = vertices[i2].pos;

		glm::vec3 edge1 = p1 - p0;
		glm::vec3 edge2 = p2 - p0;

		glm::vec3 faceNormal = glm::cross(edge1, edge2);

		if (glm::length(faceNormal) > 0)
		{
			vertices[i0].normal += faceNormal;
			vertices[i1].normal += faceNormal;
			vertices[i2].normal += faceNormal;
		}
		else
		{
			Wolf::Debug::sendWarning("Face normal is wrong, are points collinear?");
		}
	}

	for (Vertex3D& vertex : vertices)
	{
		if (glm::length(vertex.normal) > 0.0f)
		{
			vertex.normal = glm::normalize(vertex.normal);
		}
		else
		{
			Wolf::Debug::sendWarning("No contribution to the normal, the vertex may not be used");
			vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}
}

namespace std
{
	template<> struct hash<Vertex3D>
	{
		size_t operator()(Vertex3D const& vertex) const noexcept
		{
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}