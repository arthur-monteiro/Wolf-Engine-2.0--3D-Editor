#include "MeshResourceEditor.h"

#include <ConfigurationHelper.h>

#include "EditorConfiguration.h"
#include "MathsUtilsEditor.h"

MeshResourceEditor::MeshResourceEditor(const std::string& filepath, const std::function<void(ComponentInterface*)>& requestReloadCallback, bool isMeshCentered) : m_requestReloadCallback(requestReloadCallback)
{
	m_filepath = filepath;

	if (isMeshCentered)
	{
		m_isCenteredLabel.setName("Mesh is centered");
	}
	else
	{
		m_isCenteredLabel.setName("Mesh is not centered");
	}
}

void MeshResourceEditor::addShape(Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>& shape)
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

void MeshResourceEditor::activateParams()
{
	for (EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->activate();
	}
}

void MeshResourceEditor::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	for (const EditorParamInterface* editorParam : m_editorParams)
	{
		editorParam->addToJSON(outJSON, tabCount, false);
	}
}

void MeshResourceEditor::computeOutputJSON(std::string& out)
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

void MeshResourceEditor::computeInfoForRectangle(uint32_t meshIdx, glm::vec3& outP0, glm::vec3& outS1, glm::vec3& outS2)
{
	computeRectanglePointsFromScaleRotationOffset(m_physicsMeshes[meshIdx].getDefaultRectangleScale(), m_physicsMeshes[meshIdx].getDefaultRectangleRotation(), m_physicsMeshes[meshIdx].getDefaultRectangleOffset(), outP0, outS1, outS2);
}

MeshResourceEditor::PhysicMesh::PhysicMesh() : ParameterGroupInterface(TAB)
{
	m_name = DEFAULT_NAME;
}

void MeshResourceEditor::PhysicMesh::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

void MeshResourceEditor::PhysicMesh::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	std::copy(m_allParams.data(), &m_allParams.back() + 1, std::back_inserter(out));
}

bool MeshResourceEditor::PhysicMesh::hasDefaultName() const
{
	return std::string(m_name) == DEFAULT_NAME;
}

Wolf::Physics::PhysicsShapeType MeshResourceEditor::PhysicMesh::getType() const
{
	return static_cast<Wolf::Physics::PhysicsShapeType>(static_cast<uint32_t>(m_shapeType));
}

void MeshResourceEditor::PhysicMesh::onValueChanged()
{
	notifySubscribers(static_cast<uint32_t>(ResourceEditorNotificationFlagBits::PHYSICS));
}

void MeshResourceEditor::onPhysicsMeshAdded()
{
	m_requestReloadCallback(this);
	m_physicsMeshes.back().subscribe(this, [this](Notifier::Flags) { notifySubscribers(static_cast<uint32_t>(ResourceEditorNotificationFlagBits::PHYSICS)); });
}

void MeshResourceEditor::centerMesh()
{
	std::string fullFilepath = g_editorConfiguration->computeFullPathFromLocalPath(m_filepath) + ".config";
	Wolf::ConfigurationHelper::writeInfoToFile(fullFilepath, "forceCenter", true);

	notifySubscribers(static_cast<uint32_t>(ResourceEditorNotificationFlagBits::MESH));
}
