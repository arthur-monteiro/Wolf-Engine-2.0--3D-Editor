#include "EditorModelInterface.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Wolf;

EditorModelInterface::EditorModelInterface(const glm::mat4& transform)
{
	m_transform = transform;
	glm::quat quatRotation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(m_transform, m_scaleParam.getValue(), quatRotation, m_translationParam.getValue(), skew, perspective);
	m_rotationParam = glm::eulerAngles(quatRotation) * 3.14159f / 180.f;

	m_matricesUniformBuffer.reset(new Buffer(sizeof(MatricesUBData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, UpdateRate::NEVER));

	m_modelDescriptorSetLayoutGenerator.reset(new LazyInitSharedResource<DescriptorSetLayoutGenerator, EditorModelInterface>([this](std::unique_ptr<DescriptorSetLayoutGenerator>& descriptorSetLayoutGenerator)
		{
			descriptorSetLayoutGenerator.reset(new DescriptorSetLayoutGenerator);
			descriptorSetLayoutGenerator->addUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0); // matrices
		}));

	m_modelDescriptorSetLayout.reset(new LazyInitSharedResource<DescriptorSetLayout, EditorModelInterface>([this](std::unique_ptr<DescriptorSetLayout>& descriptorSetLayout)
		{
			descriptorSetLayout.reset(new DescriptorSetLayout(m_modelDescriptorSetLayoutGenerator->getResource()->getDescriptorLayouts()));
		}));
}

void EditorModelInterface::updateGraphic()
{
	MatricesUBData mvp;
	mvp.model = m_transform;
	m_matricesUniformBuffer->transferCPUMemory(&mvp, sizeof(mvp), 0);
}

std::string EditorModelInterface::convertModelTypeToString(ModelType modelType)
{
	switch (modelType)
	{
		case ModelType::STATIC_MESH: 
			return "staticMesh";
		case ModelType::BUILDING:
			return "building";
	}
}

void EditorModelInterface::activateParams()
{
	for (EditorParamInterface* param : m_modelParams)
	{
		param->activate();
	}
}

void EditorModelInterface::fillJSONForParams(std::string& outJSON)
{
	outJSON += "{\n";
	outJSON += "\t" R"("params": [)" "\n";
	addParamsToJSON(outJSON, m_modelParams, true);
	outJSON += "\t]\n";
	outJSON += "}";
}

void EditorModelInterface::recomputeTransform()
{
	m_transform = glm::scale(glm::mat4(1.0f), static_cast<glm::vec3>(m_scaleParam));
	m_transform = glm::rotate(m_transform, static_cast<glm::vec3>(m_rotationParam).x, glm::vec3(1.0f, 0.0f, 0.0f));
	m_transform = glm::rotate(m_transform, static_cast<glm::vec3>(m_rotationParam).y, glm::vec3(0.0f, 1.0f, 0.0f));
	m_transform = glm::rotate(m_transform, static_cast<glm::vec3>(m_rotationParam).z, glm::vec3(0.0f, 0.0f, 1.0f));
	m_transform = glm::translate(m_transform, static_cast<glm::vec3>(m_translationParam));
}

void EditorModelInterface::addParamsToJSON(std::string& outJSON, std::span<EditorParamInterface*> params, bool isLast)
{
	for (uint32_t i = 0; i < params.size(); ++i)
	{
		EditorParamInterface* param = params[i];
		param->addToJSON(outJSON, 2, isLast && i == params.size() - 1);
	}
}
