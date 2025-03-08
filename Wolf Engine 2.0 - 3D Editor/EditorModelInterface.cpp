#include "EditorModelInterface.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "EditorParamsHelper.h"

using namespace Wolf;

EditorModelInterface::EditorModelInterface()
{
	m_transform = glm::mat4(1.0f);
	glm::quat quatRotation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(m_transform, m_scaleParam.getValue(), quatRotation, m_translationParam.getValue(), skew, perspective);
	m_rotationParam = glm::eulerAngles(quatRotation) * 3.14159f / 180.f;

	m_modelDescriptorSetLayoutGenerator.reset(new LazyInitSharedResource<DescriptorSetLayoutGenerator, EditorModelInterface>([this](ResourceUniqueOwner<DescriptorSetLayoutGenerator>& descriptorSetLayoutGenerator)
		{
			descriptorSetLayoutGenerator.reset(new DescriptorSetLayoutGenerator);
			descriptorSetLayoutGenerator->addUniformBuffer(Wolf::ShaderStageFlagBits::VERTEX, 0); // matrices
		}));

	m_modelDescriptorSetLayout.reset(new LazyInitSharedResource<DescriptorSetLayout, EditorModelInterface>([this](ResourceUniqueOwner<DescriptorSetLayout>& descriptorSetLayout)
		{
			descriptorSetLayout.reset(DescriptorSetLayout::createDescriptorSetLayout(m_modelDescriptorSetLayoutGenerator->getResource()->getDescriptorLayouts()));
		}));
}

void EditorModelInterface::updateBeforeFrame(const Wolf::Timer& globalTimer)
{
}

glm::mat3 EditorModelInterface::computeRotationMatrix() const
{
	glm::mat4 r(1.0f);
	r = glm::rotate(r, static_cast<glm::vec3>(m_rotationParam).x, glm::vec3(1.0f, 0.0f, 0.0f));
	r = glm::rotate(r, static_cast<glm::vec3>(m_rotationParam).y, glm::vec3(0.0f, 1.0f, 0.0f));
	r = glm::rotate(r, static_cast<glm::vec3>(m_rotationParam).z, glm::vec3(0.0f, 0.0f, 1.0f));

	return r;
}

void EditorModelInterface::activateParams()
{
	for (EditorParamInterface* param : m_modelParams)
	{
		param->activate();
	}
}

void EditorModelInterface::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	::addParamsToJSON(outJSON, m_modelParams, false, tabCount);
}

void EditorModelInterface::recomputeTransform()
{
	m_transform = glm::translate(glm::mat4(1.0f), static_cast<glm::vec3>(m_translationParam));
	m_transform = glm::rotate(m_transform, static_cast<glm::vec3>(m_rotationParam).x, glm::vec3(1.0f, 0.0f, 0.0f));
	m_transform = glm::rotate(m_transform, static_cast<glm::vec3>(m_rotationParam).y, glm::vec3(0.0f, 1.0f, 0.0f));
	m_transform = glm::rotate(m_transform, static_cast<glm::vec3>(m_rotationParam).z, glm::vec3(0.0f, 0.0f, 1.0f));
	m_transform = glm::scale(m_transform, static_cast<glm::vec3>(m_scaleParam));

	notifySubscribers();
}