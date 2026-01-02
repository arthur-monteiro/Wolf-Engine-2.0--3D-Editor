#include "MeshAssetEditor.h"

#include <glm/gtc/matrix_transform.hpp>

#include <ConfigurationHelper.h>

#include "ComputeVertexDataPass.h"
#include "EditorConfiguration.h"
#include "MathsUtilsEditor.h"
#include "ModelLoader.h"

MeshAssetEditor::MeshAssetEditor(const std::string& filepath, const std::function<void(ComponentInterface*)>& requestReloadCallback, ModelData* modelData, uint32_t firstMaterialIdx, const Wolf::NullableResourceNonOwner<Wolf::BottomLevelAccelerationStructure>& bottomLevelAccelerationStructure,
	const std::function<void(const std::string&)>& isolateMeshCallback, const std::function<void(glm::mat4&)>& removeIsolationAndGetViewMatrixCallback, const std::function<void(const glm::mat4&)>& requestThumbnailReload,
	const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
: m_requestReloadCallback(requestReloadCallback), m_isolateMeshCallback(isolateMeshCallback), m_removeIsolationAndGetViewMatrixCallback(removeIsolationAndGetViewMatrixCallback), m_requestThumbnailReload(requestThumbnailReload),
  m_renderingPipeline(renderingPipeline)
{
	m_filepath = filepath;

	if (modelData->m_isMeshCentered)
	{
		m_isCenteredLabel.setName("Mesh is centered");
	}
	else
	{
		m_isCenteredLabel.setName("Mesh is not centered");
	}

	LODInfo& lodInfo = m_defaultLODsInfo.emplace_back();
	lodInfo.setRenderingPipeline(m_renderingPipeline);
	lodInfo.setModelData(modelData);
	lodInfo.setFirstMaterialIdx(firstMaterialIdx);
	lodInfo.setBottomLevelAccelerationStructure(bottomLevelAccelerationStructure);
	lodInfo.setLODIndexAndType(0, 0);
	lodInfo.setError(0.0f);
	lodInfo.setIndexCount(modelData->m_mesh->getIndexCount());
	lodInfo.setName("LOD 0");

	uint32_t lodIdx = 1;
	for (ModelData::LODInfo& modelLODInfo : modelData->m_defaultLODsInfo)
	{
		LODInfo& editorLODInfo = m_defaultLODsInfo.emplace_back();
		editorLODInfo.setRenderingPipeline(m_renderingPipeline);
		editorLODInfo.setModelData(modelData);
		editorLODInfo.setFirstMaterialIdx(firstMaterialIdx);
		editorLODInfo.setBottomLevelAccelerationStructure(bottomLevelAccelerationStructure);
		editorLODInfo.setLODIndexAndType(lodIdx, 0);
		editorLODInfo.setError(modelLODInfo.m_error);
		editorLODInfo.setIndexCount(modelLODInfo.m_indexCount);
		editorLODInfo.setName("LOD " + std::to_string(lodIdx));

		lodIdx++;
	}

	if (!modelData->m_sloppyLODsInfo.empty())
	{
		uint32_t sloppyIdx = 1;
		for (ModelData::LODInfo& sloppyLODInfo : modelData->m_sloppyLODsInfo)
		{
			LODInfo& editorSloppyInfo = m_sloppyLODsInfo.emplace_back();
			editorSloppyInfo.setRenderingPipeline(m_renderingPipeline);
			editorSloppyInfo.setModelData(modelData);
			editorSloppyInfo.setFirstMaterialIdx(firstMaterialIdx);
			editorSloppyInfo.setBottomLevelAccelerationStructure(bottomLevelAccelerationStructure);
			editorSloppyInfo.setLODIndexAndType(sloppyIdx, 1);
			editorSloppyInfo.setError(sloppyLODInfo.m_error);
			editorSloppyInfo.setIndexCount(sloppyLODInfo.m_indexCount);
			editorSloppyInfo.setName("Sloppy LOD " + std::to_string(sloppyIdx));
			sloppyIdx++;
		}
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
	else if (type == Wolf::Physics::PhysicsShapeType::Box)
	{
		physicMesh.setShapeType(typeIdx);
		physicMesh.setName(shape->getName());

		Wolf::ResourceNonOwner<Wolf::Physics::Box> shapeAsBox = shape.createNonOwnerResource<Wolf::Physics::Box>();

		glm::vec3 aabbMin = shapeAsBox->getAABBMin();
		glm::vec3 aabbMax = shapeAsBox->getAABBMax();
		glm::vec3 rotation = shapeAsBox->getRotation();

		physicMesh.setDefaultBoxAABBMin(aabbMin);
		physicMesh.setDefaultBoxAABBMax(aabbMax);
		physicMesh.setDefaultBoxRotation(rotation);
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
		else if (m_physicsMeshes[i].getType() == Wolf::Physics::PhysicsShapeType::Box)
		{
			out += "\t\t\t\"name\" : \"" + static_cast<std::string>(*m_physicsMeshes[i].getNameParam()) + "\",\n";

			glm::vec3 aabbMin = m_physicsMeshes[i].getDefaultBoxAABBMin();
			glm::vec3 aabbMax = m_physicsMeshes[i].getDefaultBoxAABBMax();
			glm::vec3 rot = m_physicsMeshes[i].getDefaultBoxRotation();

			out += "\t\t\t\"defaultAABBMinX\": " + std::to_string(aabbMin.x) + ",\n";
			out += "\t\t\t\"defaultAABBMinY\": " + std::to_string(aabbMin.y) + ",\n";
			out += "\t\t\t\"defaultAABBMinZ\": " + std::to_string(aabbMin.z) + ",\n";

			out += "\t\t\t\"defaultAABBMaxX\": " + std::to_string(aabbMax.x) + ",\n";
			out += "\t\t\t\"defaultAABBMaxY\": " + std::to_string(aabbMax.y) + ",\n";
			out += "\t\t\t\"defaultAABBMaxZ\": " + std::to_string(aabbMax.z) + ",\n";

			out += "\t\t\t\"defaultRotationX\": " + std::to_string(rot.x) + ",\n";
			out += "\t\t\t\"defaultRotationY\": " + std::to_string(rot.y) + ",\n";
			out += "\t\t\t\"defaultRotationZ\": " + std::to_string(rot.z) + ",\n";
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

void MeshAssetEditor::computeInfoForBox(uint32_t meshIdx, glm::vec3& outAABBMin, glm::vec3& outAABBMax, glm::vec3& outRotation)
{
	outAABBMin = m_physicsMeshes[meshIdx].getDefaultBoxAABBMin();
	outAABBMax = m_physicsMeshes[meshIdx].getDefaultBoxAABBMax();
	outRotation = m_physicsMeshes[meshIdx].getDefaultBoxRotation();
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
	for (EditorParamInterface* param : m_alwaysVisibleParams)
	{
		out.push_back(param);
	}

	switch (static_cast<uint32_t>(m_shapeType))
	{
		case static_cast<uint32_t>(Wolf::Physics::PhysicsShapeType::Rectangle):
		{
			for (EditorParamInterface* param : m_rectangleParams)
			{
				out.push_back(param);
			}
			break;
		}
		case static_cast<uint32_t>(Wolf::Physics::PhysicsShapeType::Box):
		{
			for (EditorParamInterface* param : m_boxParams)
			{
				out.push_back(param);
			}
			break;
		}
	}
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

void MeshAssetEditor::PhysicMesh::onShapeTypeChanged()
{
	onValueChanged();
	m_requestReloadCallback(nullptr);
}

void MeshAssetEditor::onPhysicsMeshAdded()
{
	m_requestReloadCallback(this);
	m_physicsMeshes.back().setRequestReloadCallback(m_requestReloadCallback);
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

void MeshAssetEditor::LODInfo::setLODIndexAndType(uint32_t lodIdx, uint32_t lodType)
{
	m_lodIdx = lodIdx;
	m_lodType = lodType;
}

void MeshAssetEditor::LODInfo::onComputeVertexColorsAndNormals()
{
	Wolf::NullableResourceNonOwner<ComputeVertexDataPass> computeVertexDataPass = m_renderingPipeline->getComputeVertexDataPass();

	if  (!computeVertexDataPass)
	{
		Wolf::Debug::sendWarning("Compute vertex data pass not initialized, can't compute vertex colors and normals");
		return;
	}

	Wolf::NullableResourceNonOwner<Wolf::Buffer> overrideIndexBuffer;
	if (m_lodIdx > 0)
	{
		if (m_lodType == 0)
		{
			overrideIndexBuffer = m_modelData->m_defaultSimplifiedIndexBuffers[m_lodIdx - 1].createNonOwnerResource();
		}
		else if (m_lodType == 1)
		{
			overrideIndexBuffer = m_modelData->m_sloppySimplifiedIndexBuffers[m_lodIdx - 1].createNonOwnerResource();
		}
		else
		{
			Wolf::Debug::sendError("Unhandled LOD type");
		}
	}

	ComputeVertexDataPass::Request request(m_modelData->m_mesh.createNonOwnerResource(), overrideIndexBuffer, m_firstMaterialIdx, m_bottomLevelAccelerationStructure);
	computeVertexDataPass->addRequestBeforeFrame(request);
}
