#include "MeshAssetEditor.h"

#include <ConfigurationHelper.h>

#include "EditorConfiguration.h"
#include "MathsUtilsEditor.h"
#include "ModelLoader.h"

MeshAssetEditor::MeshAssetEditor(const std::string& filepath, const std::function<void(ComponentInterface*)>& requestReloadCallback, Wolf::ModelData* modelData, const std::function<void(const std::string&)>& isolateMeshCallback,
                                 const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback, const std::function<void(const glm::mat4&)>& requestThumbnailReload)
: m_requestReloadCallback(requestReloadCallback), m_isolateMeshCallback(isolateMeshCallback), m_removeIsolationAndGetViewMatrixCallback(removeIsolationAndGetViewMatrixCallback),
  m_requestThumbnailReload(requestThumbnailReload)
{
	m_filepath = filepath;

	if (modelData->isMeshCentered)
	{
		m_isCenteredLabel.setName("Mesh is centered");
	}
	else
	{
		m_isCenteredLabel.setName("Mesh is not centered");
	}

	LODInfo& lodInfo = m_lodsInfo.emplace_back();
	lodInfo.setError(0.0f);
	lodInfo.setIndexCount(modelData->mesh->getIndexCount());
	lodInfo.setName("LOD 0");

	uint32_t lodIdx = 1;
	for (Wolf::ModelData::LODInfo& modelLODInfo : modelData->lodsInfo)
	{
		LODInfo& editorLODInfo = m_lodsInfo.emplace_back();
		editorLODInfo.setError(modelLODInfo.error);
		editorLODInfo.setIndexCount(modelLODInfo.indexCount);
		editorLODInfo.setName("LOD " + std::to_string(lodIdx));

		lodIdx++;
	}
}

void MeshAssetEditor::addShape(Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& shape)
{
	PhysicMesh& physicMesh = m_physicsMeshes.emplace_back();

	Wolf::Physics::PhysicsShapeType type = shape->getType();
	uint32_t typeIdx = static_cast<uint32_t>(type);

	if (typeIdx == static_cast<uint32_t>(-1))
	{
		Wolf::Debug::sendError("Wrong type idx");
	}

	if (type == Wolf::Physics::PhysicsShapeType::Rectangle)
	{
		physicMesh.setShapeType(typeIdx);
		physicMesh.setName(shape->getName());

		Wolf::ResourceNonOwner<Wolf::Physics::Rectangle> shapeAsRectangle = shape.createNonOwnerResource<Wolf::Physics::Rectangle>();

		glm::vec2 scale;
		glm::vec3 rotation;
		glm::vec3 offset;
		computeRectangleScaleRotationOffsetFromPoints(shapeAsRectangle->getP0(), shapeAsRectangle->getS1(), shapeAsRectangle->getS2(), scale, rotation, offset);

		physicMesh.setDefaultRectangleScale(scale);
		physicMesh.setDefaultRectangleRotation(rotation);
		physicMesh.setDefaultRectangleOffset(offset);
	}
	else
	{
		Wolf::Debug::sendError("Unhandled physics shape type");
	}
}

void MeshAssetEditor::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void MeshAssetEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void MeshAssetEditor::computePhysicsOutputJSON(std::string& out)
{
	out += "{\n";
	out += "\t\"physicsMeshesCount\": " + std::to_string(m_physicsMeshes.size()) + ",\n";
	out += "\t\"physicsMeshes\": [\n";
	for (uint32_t i = 0; i < m_physicsMeshes.size(); ++i)
	{
		out += "\t\t{\n";
		out += "\t\t\t\"type\": \"" + Wolf::Physics::PHYSICS_SHAPE_STRING_LIST[static_cast<uint32_t>(m_physicsMeshes[i].getType())] + "\",\n";

		if (m_physicsMeshes[i].getType() == Wolf::Physics::PhysicsShapeType::Rectangle)
		{
			out += "\t\t\t\"name\" : \"" + static_cast<std::string>(*m_physicsMeshes[i].getNameParam()) + "\",\n";

			glm::vec3 p0;
			glm::vec3 s1;
			glm::vec3 s2;
			computeRectanglePointsFromScaleRotationOffset(m_physicsMeshes[i].getDefaultRectangleScale(), m_physicsMeshes[i].getDefaultRectangleRotation(), m_physicsMeshes[i].getDefaultRectangleOffset(), p0, s1, s2);

			out += "\t\t\t\"defaultP0X\": " + std::to_string(p0.x) + ",\n";
			out += "\t\t\t\"defaultP0Y\": " + std::to_string(p0.y) + ",\n";
			out += "\t\t\t\"defaultP0Z\": " + std::to_string(p0.z) + ",\n";

			out += "\t\t\t\"defaultS1X\": " + std::to_string(s1.x) + ",\n";
			out += "\t\t\t\"defaultS1Y\": " + std::to_string(s1.y) + ",\n";
			out += "\t\t\t\"defaultS1Z\": " + std::to_string(s1.z) + ",\n";

			out += "\t\t\t\"defaultS2X\": " + std::to_string(s2.x) + ",\n";
			out += "\t\t\t\"defaultS2Y\": " + std::to_string(s2.y) + ",\n";
			out += "\t\t\t\"defaultS2Z\": " + std::to_string(s2.z) + ",\n";
		}
		else
		{
			Wolf::Debug::sendError("Unhandled physics mesh type");
		}
		out += "\t\t}";

		if (i != m_physicsMeshes.size() - 1)
			out += ",";
		out += "\n";
	}
	out += "\t]\n";
	out += "}\n";
}

void MeshAssetEditor::computeInfoForRectangle(uint32_t meshIdx, glm::vec3& outP0, glm::vec3& outS1, glm::vec3& outS2)
{
	computeRectanglePointsFromScaleRotationOffset(m_physicsMeshes[meshIdx].getDefaultRectangleScale(), m_physicsMeshes[meshIdx].getDefaultRectangleRotation(), m_physicsMeshes[meshIdx].getDefaultRectangleOffset(), outP0, outS1, outS2);
}

void MeshAssetEditor::computeInfoOutputJSON(std::string& out)
{
	out += "{\n";
	out += "\t\"thumbnailGenerationInfo\": {\n";
	for (glm::length_t i = 0; i < 4; ++i)
	{
		for (glm::length_t j = 0; j < 4; ++j)
		{
			out += "\t\t\"viewMatrix" + std::to_string(i) + std::to_string(j) + "\": " + std::to_string(m_viewMatrixForThumbnail[i][j]);
			if (i != 3 || j != 3)
				out += ",";
			out += "\n";
		}
	}
	out += "\t}\n";
	out += "}\n";
}

MeshAssetEditor::PhysicMesh::PhysicMesh() : ParameterGroupInterface(TAB)
{
	m_name = DEFAULT_NAME;
}

void MeshAssetEditor::PhysicMesh::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void MeshAssetEditor::PhysicMesh::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

bool MeshAssetEditor::PhysicMesh::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

Wolf::Physics::PhysicsShapeType MeshAssetEditor::PhysicMesh::getType() const
{
	return static_cast<Wolf::Physics::PhysicsShapeType>(static_cast<uint32_t>(m_shapeType));
}

void MeshAssetEditor::PhysicMesh::onValueChanged()
{
	notifySubscribers(static_cast<uint32_t>(ResourceEditorNotificationFlagBits::PHYSICS));
}

void MeshAssetEditor::onPhysicsMeshAdded()
{
	m_requestReloadCallback(this);
	m_physicsMeshes.back().subscribe(this, [this](Notifier::Flags) { notifySubscribers(static_cast<uint32_t>(ResourceEditorNotificationFlagBits::PHYSICS)); });
}

void MeshAssetEditor::centerMesh()
{
	std::string fullFilepath = g_editorConfiguration->computeFullPathFromLocalPath(m_filepath) + ".config";
	Wolf::ConfigurationHelper::writeInfoToFile(fullFilepath, "forceCenter", true);

	notifySubscribers(static_cast<uint32_t>(ResourceEditorNotificationFlagBits::MESH));
}

void MeshAssetEditor::toggleCustomViewForThumbnail()
{
	m_isInCustomViewForThumbnail = !m_isInCustomViewForThumbnail;

	if (m_isInCustomViewForThumbnail)
	{
		m_isolateMeshCallback(m_filepath);
		m_toggleCustomViewForThumbnail.setName("Exit view for thumbnail generation and save info");
		m_requestReloadCallback(this);
	}
	else
	{
		m_removeIsolationAndGetViewMatrixCallback(m_viewMatrixForThumbnail);
		m_requestThumbnailReload(m_viewMatrixForThumbnail);
		m_toggleCustomViewForThumbnail.setName("Enter view for thumbnail generation");
		m_requestReloadCallback(this);
	}
}

MeshAssetEditor::LODInfo::LODInfo() : ParameterGroupInterface(TAB)
{
	m_name = DEFAULT_NAME;
}

void MeshAssetEditor::LODInfo::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void MeshAssetEditor::LODInfo::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

bool MeshAssetEditor::LODInfo::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}
