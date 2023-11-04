#include "ModelInterface.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "DefaultPipelineInfos.h"

using namespace Wolf;

ModelInterface::ModelInterface(const glm::mat4& transform)
{
	m_transform = transform;
	glm::quat quatRotation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(m_transform, m_scale, quatRotation, m_translation, skew, perspective);
	m_rotation = glm::eulerAngles(quatRotation) * 3.14159f / 180.f;

	m_matricesUniformBuffer.reset(new Buffer(sizeof(DefaultPipeline::MatricesUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, UpdateRate::NEVER));
}

void ModelInterface::updateGraphic(const Wolf::CameraInterface& camera)
{
	DefaultPipeline::MatricesUBData mvp;
	mvp.projection = camera.getProjectionMatrix();
	mvp.view = camera.getViewMatrix();
	mvp.model = m_transform;
	m_matricesUniformBuffer->transferCPUMemory(&mvp, sizeof(mvp), 0);
}

void ModelInterface::setScale(uint32_t componentIdx, float value)
{
	m_scale[componentIdx] = value;
	recomputeTransform();
}

void ModelInterface::setRotation(uint32_t componentIdx, float value)
{
	m_rotation[componentIdx] = value;
	recomputeTransform();
}

void ModelInterface::setTranslation(uint32_t componentIdx, float value)
{
	m_translation[componentIdx] = value;
	recomputeTransform();
}

std::string ModelInterface::convertModelTypeToString(ModelType modelType)
{
	switch (modelType)
	{
		case ModelType::STATIC_MESH: 
			return "staticMesh";
		case ModelType::BUILDING:
			return "building";
	}
}

void ModelInterface::recomputeTransform()
{
	m_transform = glm::scale(glm::mat4(1.0f), m_scale);
	m_transform = glm::rotate(m_transform, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	m_transform = glm::rotate(m_transform, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	m_transform = glm::rotate(m_transform, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
	m_transform = glm::translate(m_transform, m_translation);
}
