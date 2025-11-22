#pragma once

#include "EditorTypes.h"
#include "EditorTypesTemplated.h"
#include "Entity.h"
#include "Notifier.h"
#include "ParameterGroupInterface.h"
#include "PhysicShapes.h"

namespace Wolf
{
	struct ModelData;
}

class MeshAssetEditor : public ComponentInterface
{
public:
	enum class ResourceEditorNotificationFlagBits : uint32_t
	{
		PHYSICS = 1 << 0,
		MESH = 1 << 1
	};

	std::string getId() const override { return "meshResourceEditor"; }
	MeshAssetEditor(const std::string& filepath, const std::function<void(ComponentInterface*)>& requestReloadCallback, Wolf::ModelData* modelData, const std::function<void(const std::string&)>& isolateMeshCallback,
		const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback, const std::function<void(const glm::mat4&)>& requestThumbnailReload);

	void loadParams(Wolf::JSONReader& jsonReader) override {}
	void addShape(Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& shape);

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	void saveCustom() const override {}

	void computePhysicsOutputJSON(std::string& out);
	uint32_t getPhysicsMeshCount() const { return static_cast<uint32_t>(m_physicsMeshes.size()); }
	Wolf::Physics::PhysicsShapeType getShapeType(uint32_t meshIdx) const { return m_physicsMeshes[meshIdx].getType(); }
	std::string getShapeName(uint32_t meshIdx) const { return m_physicsMeshes[meshIdx].getName(); }
	void computeInfoForRectangle(uint32_t meshIdx, glm::vec3& outP0, glm::vec3& outS1, glm::vec3& outS2);

	void computeInfoOutputJSON(std::string& out);

private:
	inline static const std::string TAB = "Mesh Resource";

	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	std::function<void(const std::string&)> m_isolateMeshCallback;
	std::function<void(glm::mat4&)> m_removeIsolationAndGetViewMatrixCallback;
	std::function<void(const glm::mat4&)> m_requestThumbnailReload;

	EditorParamString m_filepath = EditorParamString("Filepath", TAB, "General", EditorParamString::ParamStringType::STRING, false, false, true);

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

	EditorLabel m_isCenteredLabel = EditorLabel("Placeholder", TAB, "Mesh");
	void centerMesh();
	EditorParamButton m_centerMesh = EditorParamButton("Center mesh", TAB, "Mesh", [this]() { centerMesh(); });

	void toggleCustomViewForThumbnail();
	bool m_isInCustomViewForThumbnail = false;
	glm::mat4 m_viewMatrixForThumbnail{};
	EditorParamButton m_toggleCustomViewForThumbnail = EditorParamButton("Enter view for thumbnail generation", TAB, "Thumbnail", [this]() { toggleCustomViewForThumbnail(); });

	class LODInfo : public ParameterGroupInterface
	{
	public:
		LODInfo();

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override;

		void setError(float error) { m_error = std::to_string(error * 100) + "%"; }
		void setIndexCount(uint32_t indexCount) { m_indexCount = std::to_string(indexCount); }

	private:
		inline static const std::string DEFAULT_NAME = "PLACEHOLDER";

		EditorParamString m_error = EditorParamString("Error", TAB, "Info", EditorParamString::ParamStringType::STRING, false, false, true);
		EditorParamString m_indexCount = EditorParamString("Index count", TAB, "Info", EditorParamString::ParamStringType::STRING, false, false, true);

		std::array<EditorParamInterface*, 2> m_allParams =
		{
			&m_error,
			&m_indexCount
		};
	};

	EditorParamArray<LODInfo> m_lodsInfo = EditorParamArray<LODInfo>("LODs", TAB, "LOD", 16, false, true);

	std::array<EditorParamInterface*, 6> m_editorParams =
	{
		&m_filepath,

		&m_physicsMeshes,

		&m_isCenteredLabel,
		&m_centerMesh,

		&m_toggleCustomViewForThumbnail,

		&m_lodsInfo
	};
};

