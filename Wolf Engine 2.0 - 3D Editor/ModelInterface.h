#pragma once

#include <Buffer.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <LazyInitSharedResource.h>
#include <WolfEngine.h>

namespace Wolf
{
	class CommandBuffer;
	class CameraInterface;
	class AABB;
}

class ModelInterface
{
public:
	ModelInterface(const glm::mat4& transform);
	virtual ~ModelInterface() = default;

	virtual void updateGraphic(const Wolf::CameraInterface& camera);
	virtual void addMeshesToRenderList(Wolf::RenderMeshList&) const = 0;

	virtual const Wolf::AABB& getAABB() const = 0;
	virtual const std::string& getName() const { return m_name; }
	virtual const std::string& getLoadingPath() const = 0;
	virtual const glm::mat4& getTransform() const { return m_transform; }

	virtual void setScale(uint32_t componentIdx, float value);
	virtual void setRotation(uint32_t componentIdx, float value);
	virtual void setTranslation(uint32_t componentIdx, float value);

	enum class ModelType { STATIC_MESH, BUILDING };
	virtual ModelType getType() = 0;
	static std::string convertModelTypeToString(ModelType modelType);

private:
	void recomputeTransform();

protected:
	std::string m_name;

	glm::mat4 m_transform;
	glm::vec3 m_scale, m_translation, m_rotation;

	struct MatricesUBData
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
	};

	std::unique_ptr<Wolf::DescriptorSet> m_descriptorSet;
	std::unique_ptr<Wolf::Buffer> m_matricesUniformBuffer;

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayoutGenerator, ModelInterface>> m_modelDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, ModelInterface>> m_modelDescriptorSetLayout;
};