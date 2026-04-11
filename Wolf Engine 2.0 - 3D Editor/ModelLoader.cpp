#include "ModelLoader.h"

#include <filesystem>
#ifndef __ANDROID__
#include <meshoptimizer.h>
#endif
#define TINYOBJLOADER_IMPLEMENTATION

#include <tiny_obj_loader.h>

#include "AndroidCacheHelper.h"
#include "AssetManager.h"
#include "CodeFileHashes.h"
#include "ConfigurationHelper.h"
#include "DAEImporter.h"
#include "EditorConfiguration.h"
#include "ImageFileLoader.h"
#include "JSONReader.h"
#include "TextureSetLoader.h"
#include "MaterialsGPUManager.h"
#include "GLTFImporter.h"

inline void writeStringToCache(const std::string& str, std::fstream& outCacheFile)
{
	uint32_t strLength = static_cast<uint32_t>(str.size());
	outCacheFile.write(reinterpret_cast<char*>(&strLength), sizeof(uint32_t));
	outCacheFile.write(str.c_str(), strLength);
}

inline std::string readStringFromCache(std::ifstream& input)
{
	std::string str;
	uint32_t strLength;
	input.read(reinterpret_cast<char*>(&strLength), sizeof(uint32_t));
	str.resize(strLength);
	input.read(str.data(), strLength);

	return str;
}

void ModelLoader::loadObject(ModelData& outputModel, ModelLoadingInfo& modelLoadingInfo, const Wolf::ResourceReference<AssetManager>& assetManager)
{
#ifdef MATERIAL_DEBUG
	outputModel.m_originFilepath = modelLoadingInfo.filename;
#endif

	std::string filenameExtension = modelLoadingInfo.filename.substr(modelLoadingInfo.filename.find_last_of(".") + 1);
	if (filenameExtension == "obj")
	{
		ModelLoader objLoader(outputModel, modelLoadingInfo, assetManager);
	}
	else if (filenameExtension == "dae")
	{
		DAEImporter daeImporter(outputModel, modelLoadingInfo);
	}
	else
	{
		Wolf::Debug::sendCriticalError("Unhandled filename extension");
	}

	// Load physics
	std::string physicsConfigFilename = modelLoadingInfo.filename + ".physics.json";
	if (std::filesystem::exists(physicsConfigFilename))
	{
		Wolf::JSONReader physicsJSONReader(Wolf::JSONReader::FileReadInfo{ physicsConfigFilename });

		uint32_t physicsMeshesCount = static_cast<uint32_t>(physicsJSONReader.getRoot()->getPropertyFloat("physicsMeshesCount"));
		outputModel.m_physicsShapes.resize(physicsMeshesCount);

		for (uint32_t i = 0; i < physicsMeshesCount; ++i)
		{
			Wolf::JSONReader::JSONObjectInterface* physicsMeshObject = physicsJSONReader.getRoot()->getArrayObjectItem("physicsMeshes", i);
			std::string type = physicsMeshObject->getPropertyString("type");

			if (type == "Rectangle")
			{
				glm::vec3 p0 = glm::vec3(physicsMeshObject->getPropertyFloat("defaultP0X"), physicsMeshObject->getPropertyFloat("defaultP0Y"), physicsMeshObject->getPropertyFloat("defaultP0Z"));
				glm::vec3 s1 = glm::vec3(physicsMeshObject->getPropertyFloat("defaultS1X"), physicsMeshObject->getPropertyFloat("defaultS1Y"), physicsMeshObject->getPropertyFloat("defaultS1Z"));
				glm::vec3 s2 = glm::vec3(physicsMeshObject->getPropertyFloat("defaultS2X"), physicsMeshObject->getPropertyFloat("defaultS2Y"), physicsMeshObject->getPropertyFloat("defaultS2Z"));

				outputModel.m_physicsShapes[i].reset(new Wolf::Physics::Rectangle(physicsMeshObject->getPropertyString("name"), p0, s1, s2));
			}
			else if (type == "Box")
			{
				glm::vec3 aabbMin = glm::vec3(physicsMeshObject->getPropertyFloat("defaultAABBMinX"), physicsMeshObject->getPropertyFloat("defaultAABBMinY"), physicsMeshObject->getPropertyFloat("defaultAABBMinZ"));
				glm::vec3 aabbMax = glm::vec3(physicsMeshObject->getPropertyFloat("defaultAABBMaxX"), physicsMeshObject->getPropertyFloat("defaultAABBMaxY"), physicsMeshObject->getPropertyFloat("defaultAABBMaxZ"));
				glm::vec3 rotation = glm::vec3(physicsMeshObject->getPropertyFloat("defaultRotationX"), physicsMeshObject->getPropertyFloat("defaultRotationY"), physicsMeshObject->getPropertyFloat("defaultRotationZ"));

				outputModel.m_physicsShapes[i].reset(new Wolf::Physics::Box(physicsMeshObject->getPropertyString("name"), aabbMin, aabbMax, rotation));
			}
			else
			{
				Wolf::Debug::sendCriticalError("Unhandled physics mesh type");
			}
		}
	}
}

ModelLoader::ModelLoader(ModelData& outputModel, ModelLoadingInfo& modelLoadingInfo, const Wolf::ResourceReference<AssetManager>& assetManager) : m_outputModel(&outputModel), m_assetManager(assetManager)
{
#ifdef __ANDROID__
    if(modelLoadingInfo.isInAssets)
    {
        const std::string appFolderName = "model_cache";
        copyCompressedFileToStorage(modelLoadingInfo.filename, appFolderName, modelLoadingInfo.filename);
    }
#endif

	Wolf::Debug::sendInfo("Start loading " + modelLoadingInfo.filename);

	Wolf::Debug::sendInfo("Loading without cache");

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err, warn;

	std::string fullFilePath = g_editorConfiguration->computeFullPathFromLocalPath(modelLoadingInfo.filename);
	std::string fullMtlFolderPath = g_editorConfiguration->computeFullPathFromLocalPath(modelLoadingInfo.mtlFolder);

	if (!LoadObj(&attrib, &shapes, &materials, &warn, &err, fullFilePath.c_str(), fullMtlFolderPath.c_str()))
		Wolf::Debug::sendCriticalError(err);

	if (!err.empty())
		Wolf::Debug::sendInfo("[Loading object file] Error : " + err + " for " + modelLoadingInfo.filename + " !");
	if (!warn.empty())
		Wolf::Debug::sendInfo("[Loading object file] Warning : " + warn + " for " + modelLoadingInfo.filename + " !");

	std::unordered_map<Vertex3D, uint32_t> uniqueVertices = {};
	std::vector<InternalShapeInfo> shapeInfos(shapes.size());

	bool hasEncounteredAnInvalidMaterialId = false;
	std::map<int32_t /* materialId */, uint32_t /* subMeshIdx */> m_materialIdToSubMeshIdx;

	for(uint32_t shapeIdx = 0; shapeIdx < shapes.size(); ++shapeIdx)
	{
		auto& [name, mesh, lines, points] = shapes[shapeIdx];
		shapeInfos[shapeIdx].indicesOffset = static_cast<uint32_t>(m_outputModel->m_indices.size());

		int numVertex = 0;
		for (const auto& index : mesh.indices)
		{
			Vertex3D vertex = {};

			vertex.pos =
			{
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.color =
			{
				sRGBtoLinear(attrib.colors[3 * index.vertex_index + 0]),
				sRGBtoLinear(attrib.colors[3 * index.vertex_index + 1]),
				sRGBtoLinear(attrib.colors[3 * index.vertex_index + 2])
			};

			vertex.texCoord =
			{
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			if (attrib.normals.size() <= 3 * index.normal_index + 2)
				vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
			else
			{
				vertex.normal =
				{
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]
				};
			}

			int32_t materialId = mesh.material_ids[numVertex / 3];
			if (materialId < 0)
			{
				materialId = -1;
				hasEncounteredAnInvalidMaterialId = true;
			}

			if (!m_materialIdToSubMeshIdx.contains(materialId))
			{
				m_materialIdToSubMeshIdx[materialId] = static_cast<uint32_t>(m_materialIdToSubMeshIdx.size());
			}
			
			vertex.subMeshIdx = m_materialIdToSubMeshIdx[materialId];

			if (!uniqueVertices.contains(vertex))
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(m_outputModel->m_staticVertices.size());
				m_outputModel->m_staticVertices.push_back(vertex);
			}
			
			m_outputModel->m_indices.push_back(uniqueVertices[vertex]);
			numVertex++;
		}
	}

	if (hasEncounteredAnInvalidMaterialId)
		Wolf::Debug::sendWarning("Loading model " + modelLoadingInfo.filename + ", invalid material ID found. Switching to default (0)");

	std::array<Vertex3D, 3> tempTriangle{};
	for (size_t i(0); i <= m_outputModel->m_indices.size(); ++i)
	{
		if (i != 0 && i % 3 == 0)
		{
			glm::vec3 edge1 = tempTriangle[1].pos - tempTriangle[0].pos;
			glm::vec3 edge2 = tempTriangle[2].pos - tempTriangle[0].pos;
			glm::vec2 deltaUV1 = tempTriangle[1].texCoord - tempTriangle[0].texCoord;
			glm::vec2 deltaUV2 = tempTriangle[2].texCoord - tempTriangle[0].texCoord;

			float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

			glm::vec3 tangent;
			tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
			tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
			tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
			tangent = normalize(tangent);

			for (int32_t j(static_cast<int32_t>(i) - 1); j >= static_cast<int32_t>(i - 3); --j)
				m_outputModel->m_staticVertices[m_outputModel->m_indices[j]].tangent = tangent;
		}

		if (i == m_outputModel->m_indices.size())
			break;

		tempTriangle[i % 3] = m_outputModel->m_staticVertices[m_outputModel->m_indices[i]];
	}

	// for (uint32_t shapeIdx = 0; shapeIdx < shapeInfos.size(); ++shapeIdx)
	// {
	// 	const InternalShapeInfo& shapeInfo = shapeInfos[shapeIdx];
	// 	const InternalShapeInfo& nextShapeInfo = shapeIdx == shapeInfos.size() - 1 ? InternalShapeInfo{ static_cast<uint32_t>(indices.size()) } : shapeInfos[shapeIdx + 1];
	//
	// 	m_outputModel->m_mesh->addSubMesh(shapeInfo.indicesOffset, nextShapeInfo.indicesOffset - shapeInfo.indicesOffset);
	// }

	Wolf::Debug::sendInfo("Model " + modelLoadingInfo.filename + " loaded with " + std::to_string(m_outputModel->m_indices.size() / 3) + " triangles and " + std::to_string(shapeInfos.size()) + " shapes");

	if (modelLoadingInfo.textureSetLayout != TextureSetLoader::InputTextureSetLayout::NO_MATERIAL)
	{
		// Special case when there's no material (obj loader creates a 'none' one)
		if (materials.size() == 1 && materials[0].diffuse_texname.empty())
		{
			materials.clear();
		}

		m_outputModel->m_textureSets.resize(materials.size());

		uint32_t textureSetIdx = 0;
		for (uint32_t subMeshIdx = 0; subMeshIdx < m_materialIdToSubMeshIdx.size(); ++subMeshIdx)
		{
			int32_t tinyObjMaterialIdx = 0;
			for (; tinyObjMaterialIdx < static_cast<int32_t>(materials.size()); ++tinyObjMaterialIdx)
			{
				if (m_materialIdToSubMeshIdx[tinyObjMaterialIdx] == subMeshIdx)
					break;
			}

			if (tinyObjMaterialIdx == static_cast<int32_t>(materials.size()))
				continue; // the submesh probably don't have a material

			m_outputModel->m_textureSets[textureSetIdx] = createTextureSetFileInfoGGXFromTinyObjMaterial(materials[tinyObjMaterialIdx], modelLoadingInfo.mtlFolder);

			textureSetIdx++;
		}
	}
}

inline std::string getTexName(const std::string& texName, const std::string& folder)
{
	return !texName.empty() ? folder + "/" + texName : "";
}

TextureSetLoader::TextureSetFileInfoGGX ModelLoader::createTextureSetFileInfoGGXFromTinyObjMaterial(const tinyobj::material_t& material, const std::string& mtlFolder)
{
	TextureSetLoader::TextureSetFileInfoGGX materialFileInfo{};
	materialFileInfo.name = material.name;
	materialFileInfo.albedo = EditorConfiguration::sanitizeFilePath(getTexName(material.diffuse_texname, mtlFolder));
	materialFileInfo.normal =  EditorConfiguration::sanitizeFilePath(getTexName(material.bump_texname, mtlFolder));
	materialFileInfo.roughness =  EditorConfiguration::sanitizeFilePath(getTexName(material.specular_highlight_texname, mtlFolder));
	materialFileInfo.metalness =  EditorConfiguration::sanitizeFilePath(getTexName(material.specular_texname, mtlFolder));
	materialFileInfo.ao = EditorConfiguration::sanitizeFilePath(getTexName(material.ambient_texname, mtlFolder));
	materialFileInfo.anisoStrength = EditorConfiguration::sanitizeFilePath(getTexName(material.sheen_texname, mtlFolder));

	return materialFileInfo;
}

float ModelLoader::sRGBtoLinear(float component)
{
	// Clamp the input to the valid range [0.0, 1.0]
	component = std::max(0.0f, std::min(1.0f, component));

	if (component <= 0.04045f)
	{
		// Linear segment for dark colors
		return component / 12.92f;
	}
	else
	{
		// Gamma segment for bright colors (power function)
		return std::pow((component + 0.055f) / 1.055f, 2.4f);
	}
}
