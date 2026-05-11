#pragma once

#include "EditorTypes.h"
#include "EditorTypesTemplated.h"
#include "Entity.h"
#include "MeshFormatter.h"
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

	std::string getId() const override { return "meshAssetEditor"; }
	MeshAssetEditor(const std::string& name, const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback,
		const std::function<void(const glm::mat4&)>& requestThumbnailReload, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline,
		const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU);
	~MeshAssetEditor() override = default;

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
	void computeInfoForBox(uint32_t meshIdx, glm::vec3& outAABBMin, glm::vec3& outAABBMax, glm::vec3& outRotation);

	void computeInfoOutputJSON(std::string& out);

	void setIsCentered(bool isCentered);
	struct AddLODInfo
	{
		Wolf::NullableResourceNonOwner<Wolf::Mesh> m_mesh;
		Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> m_blas;
		uint32_t m_materialIdx;

		uint32_t m_lodType;
		uint32_t m_lodIdx;
		float m_error;
	};
	void addLOD(const AddLODInfo& addLodInfo);

private:
	inline static const std::string TAB = "Mesh asset";

	std::function<void(const std::string&)> m_isolateMeshCallback;
	std::function<void(glm::mat4&)> m_removeIsolationAndGetViewMatrixCallback;
	std::function<void(const glm::mat4&)> m_requestThumbnailReload;
	Wolf::ResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;
	Wolf::ResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;

	EditorParamString m_name = EditorParamString("Name", TAB, "General", EditorParamString::ParamStringType::STRING, false, true);

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

		void setDefaultBoxAABBMin(const glm::vec3& value) { m_defaultBoxAABBMin = value; }
		void setDefaultBoxAABBMax(const glm::vec3& value) { m_defaultBoxAABBMax = value; }
		void setDefaultBoxRotation(const glm::vec3& value) { m_defaultBoxRotation = value; }

		Wolf::Physics::PhysicsShapeType getType() const;
		glm::vec2 getDefaultRectangleScale() const { return m_defaultRectangleScale; }
		glm::vec3 getDefaultRectangleRotation() const { return m_defaultRectangleRotation; }
		glm::vec3 getDefaultRectangleOffset() const { return m_defaultRectangleOffset; }

		glm::vec3 getDefaultBoxAABBMin() const { return m_defaultBoxAABBMin; }
		glm::vec3 getDefaultBoxAABBMax() const { return m_defaultBoxAABBMax; }
		glm::vec3 getDefaultBoxRotation() const { return m_defaultBoxRotation; }

	private:
		inline static const std::string DEFAULT_NAME = "New physic mesh";

		void onValueChanged();

		void onShapeTypeChanged();
		EditorParamEnum m_shapeType = EditorParamEnum(Wolf::Physics::PHYSICS_SHAPE_STRING_LIST, "Shape type", TAB, "Shape", [this]() { onShapeTypeChanged(); });

		// Rectangle
		EditorParamVector2 m_defaultRectangleScale = EditorParamVector2("Default scale", TAB, "Shape", 0.0f, 10.0f, [this]() { onValueChanged(); });
		EditorParamVector3 m_defaultRectangleRotation = EditorParamVector3("Default rotation", TAB, "Shape", 0.0f, 6.29f, [this]() { onValueChanged(); });
		EditorParamVector3 m_defaultRectangleOffset = EditorParamVector3("Default offset", TAB, "Shape", 0.0f, 6.29f, [this]() { onValueChanged(); });

		// Box
		EditorParamVector3 m_defaultBoxAABBMin = EditorParamVector3("Default AABB Min", TAB, "Shape", -100.0f, 100.0f, [this]() { onValueChanged(); });
		EditorParamVector3 m_defaultBoxAABBMax = EditorParamVector3("Default AABB Max", TAB, "Shape", -100.0f, 100.0f, [this]() { onValueChanged(); });
		EditorParamVector3 m_defaultBoxRotation = EditorParamVector3("Default rotation (box)", TAB, "Shape", 0.0f, 6.29f, [this]() { onValueChanged(); });

		std::array<EditorParamInterface*, 7> m_allParams =
		{
			&m_shapeType,
			&m_defaultRectangleScale,
			&m_defaultRectangleRotation,
			&m_defaultRectangleOffset,
			&m_defaultBoxAABBMin,
			&m_defaultBoxAABBMax,
			&m_defaultBoxRotation
		};

		std::array<EditorParamInterface*, 1> m_alwaysVisibleParams =
		{
			&m_shapeType
		};

		std::array<EditorParamInterface*, 3> m_rectangleParams =
		{
			&m_defaultRectangleScale,
			&m_defaultRectangleRotation,
			&m_defaultRectangleOffset
		};

		std::array<EditorParamInterface*, 3> m_boxParams =
		{
			&m_defaultBoxAABBMin,
			&m_defaultBoxAABBMax,
			&m_defaultBoxRotation
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

		void setRenderingPipeline(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline) { m_renderingPipeline = renderingPipeline; }
		void setEditorPushDataToGPU(const Wolf::ResourceNonOwner<EditorGPUDataTransfersManager>& editorPushDataToGPU) { m_editorPushDataToGPU = editorPushDataToGPU; }
		void setMesh(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh) { m_mesh = mesh; }
		void setDefaultMaterialIdx(uint32_t defaultMaterialIdx) { m_defaultMaterialIdx = defaultMaterialIdx; }
		void setBottomLevelAccelerationStructure(const Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure>& accelerationStructure) { m_bottomLevelAccelerationStructure = accelerationStructure; }
		void setLODIndexAndType(uint32_t lodIdx, uint32_t lodType);
		void setError(float error) { m_error = std::to_string(error * 100) + "%"; }
		void setIndexCount(uint32_t indexCount) { m_indexCount = std::to_string(indexCount); }

	private:
		inline static const std::string DEFAULT_NAME = "PLACEHOLDER";

		Wolf::NullableResourceNonOwner<RenderingPipelineInterface> m_renderingPipeline;
		Wolf::NullableResourceNonOwner<EditorGPUDataTransfersManager> m_editorPushDataToGPU;
		Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure> m_bottomLevelAccelerationStructure;
		Wolf::NullableResourceNonOwner<Wolf::Mesh> m_mesh;
		uint32_t m_defaultMaterialIdx;
		uint32_t m_lodIdx = 0;
		uint32_t m_lodType = 0;

		EditorParamString m_error = EditorParamString("Error", TAB, "Info", EditorParamString::ParamStringType::STRING, false, true);
		EditorParamString m_indexCount = EditorParamString("Index count", TAB, "Info", EditorParamString::ParamStringType::STRING, false, true);

		void onComputeVertexColorsAndNormals();
		EditorParamButton m_computeVertexColorsAndNormals = EditorParamButton("Compute vertex colors and normals", TAB, "Actions", [this]() { onComputeVertexColorsAndNormals();});

		std::array<EditorParamInterface*, 3> m_allParams =
		{
			&m_error,
			&m_indexCount,
			&m_computeVertexColorsAndNormals
		};
	};

	EditorParamArray<LODInfo> m_defaultLODsInfo = EditorParamArray<LODInfo>("Default LODs", TAB, "LOD", 16, false, true);
	EditorParamArray<LODInfo> m_sloppyLODsInfo = EditorParamArray<LODInfo>("Sloppy LODs", TAB, "LOD", 32, false, true);

	std::array<EditorParamInterface*, 7> m_editorParams =
	{
		&m_name,

		&m_physicsMeshes,

		&m_isCenteredLabel,
		&m_centerMesh,

		&m_toggleCustomViewForThumbnail,

		&m_defaultLODsInfo,
		&m_sloppyLODsInfo
	};
};
