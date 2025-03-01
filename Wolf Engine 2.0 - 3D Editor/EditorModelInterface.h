#pragma once

#include <Buffer.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <LazyInitSharedResource.h>
#include <WolfEngine.h>

#include "ComponentInterface.h"
#include "DrawManager.h"
#include "EditorPhysicsManager.h"
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
	EditorModelInterface();

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	virtual bool getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) = 0;
	virtual bool getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList) = 0;

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}

	virtual Wolf::AABB getAABB() const = 0;
	virtual const glm::mat4& getTransform() const { return m_transform; }
	glm::vec3 getPosition() const { return m_translationParam; }
	glm::mat3 computeRotationMatrix() const;
	void setPosition(const glm::vec3& newPosition) { m_translationParam = newPosition; }
	void setRotation(const glm::vec3& newRotation) { m_rotationParam = newRotation; }
	
	virtual std::string getTypeString() = 0;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

private:
	void recomputeTransform();

protected:
	glm::mat4 m_transform;
	bool m_computeFromLine = false;

	EditorParamVector3 m_scaleParam = EditorParamVector3("Scale", "Model", "Transform", -1.0f, 1.0f, [this] { recomputeTransform(); });
	EditorParamVector3 m_translationParam = EditorParamVector3("Translation", "Model", "Transform", -10.0f, 10.0f, [this] { recomputeTransform(); });
	EditorParamVector3 m_rotationParam = EditorParamVector3("Rotation", "Model", "Transform", 0.0f, 6.29f, [this] { recomputeTransform(); });

	std::array<EditorParamInterface*, 3> m_modelParams =
	{
		&m_scaleParam,
		&m_translationParam,
		&m_rotationParam
	};

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayoutGenerator, EditorModelInterface>> m_modelDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, EditorModelInterface>> m_modelDescriptorSetLayout;
};