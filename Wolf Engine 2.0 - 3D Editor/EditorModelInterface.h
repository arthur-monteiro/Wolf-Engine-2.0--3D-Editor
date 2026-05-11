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
#include "RayTracedWorldManager.h"

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

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	virtual bool getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) = 0;
	virtual bool getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos) { return true; }
	virtual bool getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList) = 0;

	virtual Wolf::AABB getAABB() const = 0;
	virtual Wolf::BoundingSphere getBoundingSphere() const = 0;
	virtual const glm::mat4& getTransform() const { return m_transform; }
	glm::vec3 getPosition() const { return m_translationParam; }
	glm::mat3 computeRotationMatrix() const;
	void setPosition(const glm::vec3& newPosition) { m_translationParam = newPosition; }
	void setRotation(const glm::quat& newRotation) { m_rotationQuaternionParam = glm::vec4(newRotation.x, newRotation.y, newRotation.z, newRotation.w); }
	void setScale(const glm::vec3& newScale) { m_scaleParam = newScale; }

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

protected:
	void loadParams(Wolf::JSONReader& jsonReader, const std::string& id);

private:
	void recomputeTransform();

protected:
	glm::mat4 m_transform;
	bool m_computeFromLine = false;

	EditorParamVector3 m_scaleParam = EditorParamVector3("Scale", "Mesh", "Transform", -1.0f, 1.0f, [this] { recomputeTransform(); });
	EditorParamVector3 m_translationParam = EditorParamVector3("Translation", "Mesh", "Transform", -10.0f, 10.0f, [this] { recomputeTransform(); });

	void updateRotation();
	EditorParamVector4 m_rotationQuaternionParam = EditorParamVector4("Rotation quaternion", "Mesh", "Transform", -1.0f, 1.0f, [this] { recomputeTransform(); }, false, true);
	EditorParamVector3 m_rotationParam = EditorParamVector3("Rotation", "Mesh", "Transform", 0.0f, 6.29f, [this] { updateRotation(); });

	std::array<EditorParamInterface*, 4> m_modelParams =
	{
		&m_scaleParam,
		&m_translationParam,
		&m_rotationParam,
		&m_rotationQuaternionParam
	};

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayoutGenerator, EditorModelInterface>> m_modelDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, EditorModelInterface>> m_modelDescriptorSetLayout;
};