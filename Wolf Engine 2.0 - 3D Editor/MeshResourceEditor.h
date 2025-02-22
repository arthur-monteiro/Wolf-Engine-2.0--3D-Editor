#pragma once

#include "EditorTypesTemplated.h"
#include "Entity.h"
#include "Notifier.h"
#include "ParameterGroupInterface.h"
#include "PhysicShapes.h"

class MeshResourceEditor : public ComponentInterface, public Notifier
{
public:
	std::string getId() const override { return "meshResourceEditor"; }
	MeshResourceEditor(const std::function<void(ComponentInterface*)>& requestReloadCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override {}
	void addShape(Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& shape);

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override {}
	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	bool requiresInputs() const override { return false; }
	void saveCustom() const override {}

	void computeOutputJSON(std::string& out);
	uint32_t getMeshCount() const { return static_cast<uint32_t>(m_physicsMeshes.size()); }
	Wolf::Physics::PhysicsShapeType getShapeType(uint32_t meshIdx) const { return m_physicsMeshes[meshIdx].getType(); }
	std::string getShapeName(uint32_t meshIdx) const { return m_physicsMeshes[meshIdx].getName(); }
	void computeInfoForRectangle(uint32_t meshIdx, glm::vec3& outP0, glm::vec3& outS1, glm::vec3& outS2);

private:
	inline static const std::string TAB = "Mesh Resource";

	std::function<void(ComponentInterface*)> m_requestReloadCallback;

	class PhysicMesh : public ParameterGroupInterface, public Notifier
	{
	public:
		PhysicMesh();

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override;

		void setShapeType(uint32_t typeIdx) { m_shapeType = typeIdx; }
		void setDefaultRectangleScale(const glm::vec2& value) { m_defaultRectangleScale = value; }
		void setDefaultRectangleRotation(const glm::vec3& value) { m_defaultRectangleRotation = value; }
		void setDefaultRectangleOffset(const glm::vec3& value) { m_defaultRectangleOffset = value; }

		Wolf::Physics::PhysicsShapeType getType() const;
		glm::vec2 getDefaultRectangleScale() const { return m_defaultRectangleScale; }
		glm::vec3 getDefaultRectangleRotation() const { return m_defaultRectangleRotation; }
		glm::vec3 getDefaultRectangleOffset() const { return m_defaultRectangleOffset; }

	private:
		inline static const std::string DEFAULT_NAME = "New physic mesh";
		std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

		void onValueChanged();

		EditorParamEnum m_shapeType = EditorParamEnum(Wolf::Physics::PHYSICS_SHAPE_STRING_LIST, "Shape type", TAB, "Shape", [this]() { onValueChanged(); });

		// Rectangle
		EditorParamVector2 m_defaultRectangleScale = EditorParamVector2("Default scale", TAB, "Shape", 0.0f, 10.0f, [this]() { onValueChanged(); });
		EditorParamVector3 m_defaultRectangleRotation = EditorParamVector3("Default rotation", TAB, "Shape", 0.0f, 6.29f, [this]() { onValueChanged(); });
		EditorParamVector3 m_defaultRectangleOffset = EditorParamVector3("Default offset", TAB, "Shape", 0.0f, 6.29f, [this]() { onValueChanged(); });

		std::array<EditorParamInterface*, 4> m_allParams =
		{
			&m_shapeType,
			&m_defaultRectangleScale,
			&m_defaultRectangleRotation,
			&m_defaultRectangleOffset
		};
	};
	static constexpr uint32_t MAX_PHYSICS_MESHES = 256;
	void onPhysicsMeshAdded();
	EditorParamArray<PhysicMesh> m_physicsMeshes = EditorParamArray<PhysicMesh>("Physics meshes", TAB, "Physics", MAX_PHYSICS_MESHES, [this] { onPhysicsMeshAdded(); }, false);

	std::array<EditorParamInterface*, 1> m_editorParams =
	{
		&m_physicsMeshes
	};
};

