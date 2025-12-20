#pragma once

#include <DynamicResourceUniqueOwnerArray.h>
#include <RenderMeshList.h>
#include <ResourceUniqueOwner.h>

#include "RenderingPipelineInterface.h"

class Entity;

struct InstanceData
{
	glm::mat4 transform;
	uint32_t firstMaterialIdx;
	uint32_t entityIdx;

	static void getBindingDescription(VkVertexInputBindingDescription& bindingDescription, uint32_t binding)
	{
		bindingDescription.binding = binding;
		bindingDescription.stride = sizeof(InstanceData);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	}

	static void getAttributeDescriptions(std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, uint32_t binding)
	{
		const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
		attributeDescriptions.resize(attributeDescriptionCountBefore + 6);

		attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 0].location = attributeDescriptionCountBefore + 0;
		attributeDescriptions[attributeDescriptionCountBefore + 0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(InstanceData, transform);

		attributeDescriptions[attributeDescriptionCountBefore + 1].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 1].location = attributeDescriptionCountBefore + 1;
		attributeDescriptions[attributeDescriptionCountBefore + 1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 1].offset = offsetof(InstanceData, transform) + sizeof(glm::vec4);

		attributeDescriptions[attributeDescriptionCountBefore + 2].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 2].location = attributeDescriptionCountBefore + 2;
		attributeDescriptions[attributeDescriptionCountBefore + 2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 2].offset = offsetof(InstanceData, transform) + 2 * sizeof(glm::vec4);

		attributeDescriptions[attributeDescriptionCountBefore + 3].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 3].location = attributeDescriptionCountBefore + 3;
		attributeDescriptions[attributeDescriptionCountBefore + 3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[attributeDescriptionCountBefore + 3].offset = offsetof(InstanceData, transform) + 3 * sizeof(glm::vec4);

		attributeDescriptions[attributeDescriptionCountBefore + 4].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 4].location = attributeDescriptionCountBefore + 4;
		attributeDescriptions[attributeDescriptionCountBefore + 4].format = VK_FORMAT_R32_UINT;
		attributeDescriptions[attributeDescriptionCountBefore + 4].offset = offsetof(InstanceData, firstMaterialIdx);

		attributeDescriptions[attributeDescriptionCountBefore + 5].binding = binding;
		attributeDescriptions[attributeDescriptionCountBefore + 5].location = attributeDescriptionCountBefore + 5;
		attributeDescriptions[attributeDescriptionCountBefore + 5].format = VK_FORMAT_R32_UINT;
		attributeDescriptions[attributeDescriptionCountBefore + 5].offset = offsetof(InstanceData, entityIdx);
	}
};

class DrawManager
{
public:
	DrawManager(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline);

	struct DrawMeshInfo
	{
		Wolf::RenderMeshList::MeshToRender meshToRender;
		InstanceData instanceData;
	};
	void addMeshesToDraw(const std::vector<DrawMeshInfo>& meshesToRender, Entity* entity);
	void removeMeshesForEntity(Entity* entity);
	void clear();

	void isolateEntity(Entity* entity);
	void removeIsolation();

private:
	class InstancedMeshRegistered;
	void addInstanceDataToBuffer(InstancedMeshRegistered& instancedMeshRegistered, uint32_t instanceIdx, const InstanceData& instanceData);

	Wolf::ResourceNonOwner<Wolf::RenderMeshList> m_renderMeshList;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;

	static constexpr uint32_t MAX_INSTANCE_PER_MESH = 2048;
	class InstancedMeshRegistered
	{
	public:
		InstancedMeshRegistered(Wolf::RenderMeshList::MeshToRender meshToRender);

		uint32_t addInstance() { return m_instanceCount++; }
		void setInstancedMeshIdx(uint32_t value) { m_instancedMeshIdx = value; }
		uint32_t getInstancedMeshIdx() const { return m_instancedMeshIdx; }

		bool isSame(const Wolf::RenderMeshList::MeshToRender& otherMeshToRender) const;
		Wolf::ResourceUniqueOwner<Wolf::Buffer>& getInstanceBuffer() { return m_instanceBuffer; }
		Wolf::RenderMeshList::MeshToRender& getMeshToRender() { return m_meshToRender; }

	private:
		Wolf::RenderMeshList::MeshToRender m_meshToRender;

		Wolf::ResourceUniqueOwner<Wolf::Buffer> m_instanceBuffer;
		uint32_t m_instanceCount = 1;

		uint32_t m_instancedMeshIdx = -1;
	};
	Wolf::DynamicResourceUniqueOwnerArray<InstancedMeshRegistered, 64> m_instancedMeshesRegistered;

	struct InfoByEntity
	{
		uint32_t instancedMeshRegisteredIdx;
		uint32_t instanceIdx;
	};
	std::map<Entity*, std::vector<InfoByEntity>> m_infoByEntities;

	std::mutex m_meshMutex;
};

