#include "OBJImporter.h"

#include <filesystem>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Timer.h>

#include "EditorConfiguration.h"

OBJImporter::OBJImporter(ExternalSceneLoader::OutputData& outputData, const ExternalSceneLoader::SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager)
{
    Wolf::Timer loadingTimer("OBJImporter loading");

    std::string fullpathFilename = g_editorConfiguration->computeFullPathFromLocalPath(sceneLoadingInfo.filename);

    std::filesystem::path p(fullpathFilename);
    std::string fullFolderPath = p.parent_path().string();
    std::string localFolderPath = g_editorConfiguration->computeLocalPathFromFullPath(fullFolderPath);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err, warn;

    if (!LoadObj(&attrib, &shapes, &materials, &warn, &err, fullpathFilename.c_str(), fullFolderPath.c_str()))
        Wolf::Debug::sendCriticalError(err);

    if (!err.empty())
        Wolf::Debug::sendInfo("Loading .obj file: " + err + " for " + sceneLoadingInfo.filename);
    if (!warn.empty())
        Wolf::Debug::sendInfo("Loading object file: " + warn + " for " + sceneLoadingInfo.filename);

    for(uint32_t shapeIdx = 0; shapeIdx < shapes.size(); ++shapeIdx)
    {
        auto& [name, mesh, lines, points] = shapes[shapeIdx];

        Wolf::Debug::sendInfo("Reading shape " + std::to_string(shapeIdx) + " / " + std::to_string(shapes.size()) + ": " + name);

        struct Geometry
        {
            std::unordered_map<Vertex3D, uint32_t> m_uniqueVertices;
            std::vector<Vertex3D> m_vertices;
            std::vector<uint32_t> m_indices;
        };
        std::map<uint32_t, Geometry> geometryPerMaterial;

        uint32_t indexInIndices = 0;
        for (const auto& index : mesh.indices)
        {
            Vertex3D vertex = {};

            vertex.pos =
            {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.texcoord_index >= 0)
            {
                vertex.texCoord =
                {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else
            {
                vertex.texCoord = glm::vec2(0.0f);
            }


            if (attrib.normals.size() <= 3 * index.normal_index + 2)
            {
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            else
            {
                vertex.normal =
                {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            uint32_t materialId = mesh.material_ids[indexInIndices / 3];
            if (materialId < 0)
            {
                materialId = -1;
            }

            Geometry& geometry = geometryPerMaterial[materialId];

            if (!geometry.m_uniqueVertices.contains(vertex))
            {
                geometry.m_uniqueVertices[vertex] = static_cast<uint32_t>(geometry.m_vertices.size());
                geometry.m_vertices.push_back(vertex);
            }

            geometry.m_indices.push_back(geometry.m_uniqueVertices[vertex]);

            indexInIndices++;
        }

        for (auto& [materialId, geometry] : geometryPerMaterial)
        {
            std::array<Vertex3D, 3> tempTriangle{};
            for (size_t i = 0; i <= geometry.m_indices.size(); ++i)
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
                        geometry.m_vertices[geometry.m_indices[j]].tangent = tangent;
                }

                if (i == geometry.m_indices.size())
                    break;

                tempTriangle[i % 3] = geometry.m_vertices[geometry.m_indices[i]];
            }

            ExternalSceneLoader::MeshData& meshData = outputData.m_meshesData.emplace_back();
            meshData.m_name = name + "_matId" + std::to_string(materialId);
            meshData.m_indices = geometry.m_indices;
            meshData.m_staticVertices = geometry.m_vertices;

            ExternalSceneLoader::InstanceData& instanceData = outputData.m_instancesData.emplace_back();
            instanceData.m_materialIdx = materialId;
            instanceData.m_meshIdx = outputData.m_meshesData.size() - 1;
            instanceData.m_transform = glm::mat4(1.0f);
        }
    }

    Wolf::Debug::sendInfo("Reading materials");
    for (uint32_t materialIdx = 0; materialIdx < materials.size(); ++materialIdx)
    {
        tinyobj::material_t& tinyObjMaterial = materials[materialIdx];

        Wolf::Debug::sendInfo("Reading material " + std::to_string(materialIdx) + " / " + std::to_string(materials.size()) + ": " + tinyObjMaterial.name);

        ExternalSceneLoader::MaterialData& materialData = outputData.m_materialsData.emplace_back();
        TextureSetLoader::TextureSetFileInfoGGX& materialFileInfo = materialData.m_textureSetFileInfo;
        materialFileInfo.name = tinyObjMaterial.name;

        auto computeTexName = [](const std::string& texName, const std::string& folder)
        {
            return !texName.empty() ? folder + "/" + texName : "";
        };

        materialFileInfo.albedo = EditorConfiguration::sanitizeFilePath(computeTexName(tinyObjMaterial.diffuse_texname, localFolderPath));
        materialFileInfo.normal =  EditorConfiguration::sanitizeFilePath(computeTexName(tinyObjMaterial.bump_texname, localFolderPath));
        materialFileInfo.roughness =  EditorConfiguration::sanitizeFilePath(computeTexName(tinyObjMaterial.specular_highlight_texname, localFolderPath));
        materialFileInfo.metalness =  EditorConfiguration::sanitizeFilePath(computeTexName(tinyObjMaterial.specular_texname, localFolderPath));
        materialFileInfo.ao = EditorConfiguration::sanitizeFilePath(computeTexName(tinyObjMaterial.ambient_texname, localFolderPath));
        materialFileInfo.anisoStrength = EditorConfiguration::sanitizeFilePath(computeTexName(tinyObjMaterial.sheen_texname, localFolderPath));
    }
}
