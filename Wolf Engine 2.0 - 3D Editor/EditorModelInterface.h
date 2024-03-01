#pragma once

#include <Buffer.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <LazyInitSharedResource.h>
#include <WolfEngine.h>

#include "ComponentInterface.h"
#include "EditorTypes.h"

namespace Wolf
{
	class CommandBuffer;
	class CameraInterface;
	class AABB;
}

class EditorModelInterface : public ComponentInterface
{
public:
	EditorModelInterface(const glm::mat4& transform);

	virtual void updateGraphic();
	virtual void addMeshesToRenderList(Wolf::RenderMeshList&) const = 0;

	virtual Wolf::AABB getAABB() const = 0;
	virtual const glm::mat4& getTransform() const { return m_transform; }
	
	virtual std::string getTypeString() = 0;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

private:
	void recomputeTransform();

protected:
	glm::mat4 m_transform;

	EditorParamVector3 m_scaleParam = EditorParamVector3("Scale", "Model", "Transform", -1.0f, 1.0f, [this] { recomputeTransform(); });
	EditorParamVector3 m_translationParam = EditorParamVector3("Translation", "Model", "Transform", -10.0f, 10.0f, [this] { recomputeTransform(); });
	EditorParamVector3 m_rotationParam = EditorParamVector3("Rotation", "Model", "Transform", 0.0f, 6.29f, [this] { recomputeTransform(); });
	std::array<EditorParamInterface*, 3> m_modelParams =
	{
		&m_scaleParam,
		&m_translationParam,
		&m_rotationParam
	};

	struct MatricesUBData
	{
		glm::mat4 model;
	};

	std::unique_ptr<Wolf::DescriptorSet> m_descriptorSet;
	std::unique_ptr<Wolf::Buffer> m_matricesUniformBuffer;

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayoutGenerator, EditorModelInterface>> m_modelDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, EditorModelInterface>> m_modelDescriptorSetLayout;
};