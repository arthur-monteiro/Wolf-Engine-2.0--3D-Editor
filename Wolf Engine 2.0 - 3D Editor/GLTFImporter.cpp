#include "GLTFImporter.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <AABB.h>

#include "AssetManager.h"
#include "EditorConfiguration.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#include <tiny_gltf.h>

bool nullLoadImageData(tinygltf::Image* image, const int image_idx, std::string* err, std::string* warn, int req_width, int req_height, const unsigned char* bytes, int size, void* user_data)
{
    return true;
}

GLTFImporter::GLTFImporter(ExternalSceneLoader::OutputData& outputData, const ExternalSceneLoader::SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager)
{
    Wolf::Timer loadingTimer("GLTFImporter loading");

    std::string fullpathFilename = g_editorConfiguration->computeFullPathFromLocalPath(sceneLoadingInfo.filename);

    std::filesystem::path p(fullpathFilename);
    std::string folderPath = p.parent_path().string();

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(nullLoadImageData, nullptr);
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, fullpathFilename);
    if (!ret)
    {
        Wolf::Debug::sendError("Error loading " + sceneLoadingInfo.filename + ": " + err);
        return;
    }

    std::unordered_map<uint32_t /* gltf material idx */, uint32_t /* output material idx */> materialsMap;

    for (uint32_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++)
    {
        tinygltf::Mesh& mesh = model.meshes[meshIdx];
        Wolf::Debug::sendInfo("Reading mesh " + std::to_string(meshIdx) + " / " + std::to_string(model.meshes.size()) + ": " + mesh.name);

        for (size_t primitiveIdx = 0; primitiveIdx < mesh.primitives.size(); primitiveIdx++)
        {
            Wolf::Debug::sendInfo("Reading primitive " + std::to_string(primitiveIdx) + " / " + std::to_string(mesh.primitives.size()));

            const tinygltf::Primitive& primitive = mesh.primitives[primitiveIdx];
            int indexAccessorIdx = primitive.indices;

            if (model.materials[primitive.material].alphaMode == "BLEND")
            {
                Wolf::Debug::sendWarning("Alpha mode is " + model.materials[primitive.material].alphaMode + ", it's not currently supported. Mesh will be skipped");
                m_skippedMeshes.push_back(indexAccessorIdx);
                continue;
            }

            if (!m_meshesMap.contains(indexAccessorIdx))
            {
                uint32_t outputMeshIdx = static_cast<uint32_t>(outputData.m_meshesData.size());
                m_meshesMap[indexAccessorIdx].m_outputMeshIdx = outputMeshIdx;

                ExternalSceneLoader::MeshData& meshData = outputData.m_meshesData.emplace_back();
                meshData.m_name = (mesh.name.empty() ? "Mesh_" + std::to_string(meshIdx) : mesh.name) + "_prim_" + std::to_string(primitiveIdx);

                const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
                const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];
                const uint32_t posByteStride = posBufferView.byteStride == 0 ? sizeof(float) * 3 : posBufferView.byteStride;
                if (posByteStride != sizeof(float) * 3)
                {
                    Wolf::Debug::sendCriticalError("We assume that pos stride is 3 floats");
                }

                const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& normBufferView = model.bufferViews[normAccessor.bufferView];
                const tinygltf::Buffer& normBuffer = model.buffers[normBufferView.buffer];
                const uint32_t normByteStride = normBufferView.byteStride == 0 ? sizeof(float) * 3 : normBufferView.byteStride;
                if (normByteStride != sizeof(float) * 3)
                {
                    Wolf::Debug::sendCriticalError("We assume that normal stride is 3 floats");
                }

                const tinygltf::Accessor& texCoordsAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const tinygltf::BufferView& texCoordsBufferView = model.bufferViews[texCoordsAccessor.bufferView];
                const tinygltf::Buffer& texCoordsBuffer = model.buffers[texCoordsBufferView.buffer];
                const uint32_t texCoordsByteStride = texCoordsBufferView.byteStride == 0 ? sizeof(float) * 2 : texCoordsBufferView.byteStride;
                if (texCoordsByteStride != sizeof(float) * 2)
                {
                    Wolf::Debug::sendCriticalError("We assume that texture coords stride is 2 floats");
                }

                const tinygltf::Accessor& tangentAccessor = model.accessors[primitive.attributes.at("TANGENT")];
                const tinygltf::BufferView& tangentBufferView = model.bufferViews[tangentAccessor.bufferView];
                const tinygltf::Buffer& tangentBuffer = model.buffers[tangentBufferView.buffer];
                const uint32_t tangentByteStride = tangentBufferView.byteStride == 0 ? sizeof(float) * 4 : tangentBufferView.byteStride;
                if (tangentByteStride != sizeof(float) * 4)
                {
                    Wolf::Debug::sendCriticalError("We assume that tangent stride is 4 floats");
                }

                if (normAccessor.count != posAccessor.count || normAccessor.count != texCoordsAccessor.count || normAccessor.count != tangentAccessor.count)
                {
                    Wolf::Debug::sendCriticalError("Error when reading GLTF file");
                }

                meshData.m_staticVertices.reserve(posAccessor.count);
                for (size_t idx = 0; idx < posAccessor.count; idx++)
                {
                    Vertex3D vertex{};

                    const unsigned char* posData = posBuffer.data.data() + posAccessor.byteOffset + posBufferView.byteOffset;
                    vertex.pos = reinterpret_cast<const glm::vec3*>(posData)[idx];

                    const unsigned char* normData = normBuffer.data.data() + normAccessor.byteOffset + normBufferView.byteOffset;
                    vertex.normal = reinterpret_cast<const glm::vec3*>(normData)[idx];

                    const unsigned char* tangentData = tangentBuffer.data.data() + tangentAccessor.byteOffset + tangentBufferView.byteOffset;
                    vertex.tangent = reinterpret_cast<const glm::vec4*>(tangentData)[idx];

                    const unsigned char* texCoordsData = texCoordsBuffer.data.data() + texCoordsAccessor.byteOffset + texCoordsBufferView.byteOffset;
                    vertex.texCoord = reinterpret_cast<const glm::vec2*>(texCoordsData)[idx];

                    meshData.m_staticVertices.push_back(vertex);
                }

                const tinygltf::Accessor& indexAccessor = model.accessors[indexAccessorIdx];
                const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];
                const unsigned char* rawIndices = &(indexBuffer.data[indexAccessor.byteOffset + indexView.byteOffset]);

                meshData.m_indices.reserve(indexAccessor.count);

                for (size_t idx = 0; idx < indexAccessor.count; idx++)
                {
                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        meshData.m_indices.push_back(reinterpret_cast<const uint32_t*>(rawIndices)[idx]);
                    }
                    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        meshData.m_indices.push_back(reinterpret_cast<const uint16_t*>(rawIndices)[idx]);
                    }
                }
            }

            uint32_t outputMaterialIdx = -1;
            if (materialsMap.contains(primitive.material))
            {
                outputMaterialIdx = materialsMap[primitive.material];

                ImportedMeshInfo::MaterialIdentifier materialIdentifier(meshIdx, primitiveIdx);
                m_meshesMap[indexAccessorIdx].m_materialsMap[materialIdentifier] = outputMaterialIdx;
            }
            else
            {
                outputMaterialIdx = static_cast<uint32_t>(outputData.m_materialsData.size());
                materialsMap[primitive.material] = outputMaterialIdx;

                ImportedMeshInfo::MaterialIdentifier materialIdentifier(meshIdx, primitiveIdx);
                m_meshesMap[indexAccessorIdx].m_materialsMap[materialIdentifier] = outputMaterialIdx;

                ExternalSceneLoader::MaterialData& outputMaterialData = outputData.m_materialsData.emplace_back();
                tinygltf::Material& material = model.materials[primitive.material];
                outputMaterialData.m_textureSet.name = material.name;

                TextureSetLoader::TextureSetFileInfoGGX& materialFileInfoGGX = outputMaterialData.m_textureSetFileInfo;
                materialFileInfoGGX.name = material.name;

                std::string albedoFilename = EditorConfiguration::sanitizeFilePath(getTextureURI(model, getAlbedoTextureIndex(material)));
                materialFileInfoGGX.albedo = albedoFilename.empty() ? "" : g_editorConfiguration->computeLocalPathFromFullPath(folderPath + "/" + albedoFilename);

                std::string normalFilename = EditorConfiguration::sanitizeFilePath(getTextureURI(model, getNormalTextureIndex(material)));
                materialFileInfoGGX.normal = normalFilename.empty() ? "" : g_editorConfiguration->computeLocalPathFromFullPath(folderPath + "/" + normalFilename);

                std::string roughnessFilename = getTextureURI(model, getRoughnessTextureIndex(material));
                if (roughnessFilename.find("Roughness") != std::string::npos && roughnessFilename.find("Metalness") != std::string::npos)
                {
                    // Very hacky: for sponza intel, we get a single index for roughness and metalness but they are 2 different files. The URI we get is the merged names
                    roughnessFilename = materialFileInfoGGX.albedo;
                    size_t pos = roughnessFilename.find("BaseColor");
                    if (pos != std::string::npos)
                    {
                        roughnessFilename.replace(pos, 9, "Roughness");
                    }
                }
                else
                {
                    if (!roughnessFilename.empty())
                        roughnessFilename = g_editorConfiguration->computeLocalPathFromFullPath(folderPath + "/" + EditorConfiguration::sanitizeFilePath(roughnessFilename));
                }
                materialFileInfoGGX.roughness = roughnessFilename;

                std::string metalnessFilename = getTextureURI(model, getMetallicTextureIndex(material));
                if (metalnessFilename.find("Roughness") != std::string::npos && metalnessFilename.find("Metalness") != std::string::npos)
                {
                    // Very hacky: for sponza intel, we get a single index for roughness and metalness but they are 2 different files. The URI we get is the merged names
                    metalnessFilename = materialFileInfoGGX.albedo;
                    size_t pos = metalnessFilename.find("BaseColor");
                    if (pos != std::string::npos)
                    {
                        metalnessFilename.replace(pos, 9, "Roughness");
                    }
                }
                else
                {
                    if (!metalnessFilename.empty())
                        metalnessFilename = g_editorConfiguration->computeLocalPathFromFullPath(folderPath + "/" + EditorConfiguration::sanitizeFilePath(metalnessFilename));
                }
                materialFileInfoGGX.metalness = metalnessFilename;
                materialFileInfoGGX.ao = "";
                materialFileInfoGGX.anisoStrength = "";

                TextureSetLoader::OutputLayout outputLayout;
                outputLayout.albedoCompression = Wolf::ImageCompression::Compression::BC1;
                outputLayout.normalCompression = Wolf::ImageCompression::Compression::BC5;

                TextureSetLoader textureSetLoader(materialFileInfoGGX, outputLayout, true, assetManager);

                for (uint32_t i = 0; i < 2 /* albedo and normal */; ++i)
                {
                    if (AssetId assetId = textureSetLoader.getImageAssetId(i); assetId != NO_ASSET)
                    {
                        outputMaterialData.m_textureSet.imageAssetIds[i] = assetId;

                        outputMaterialData.m_textureSet.images[i] = assetManager->getImage(textureSetLoader.getImageAssetId(i));
                        outputMaterialData.m_textureSet.slicesFolders[i] = assetManager->getImageSlicesFolder(textureSetLoader.getImageAssetId(i));
                    }
                }

                // Get combined image
                if (AssetId assetId = textureSetLoader.getImageAssetId(2); assetId != NO_ASSET)
                {
                    outputMaterialData.m_textureSet.imageAssetIds[2] = assetId;

                    outputMaterialData.m_textureSet.images[2] = assetManager->getCombinedImage(textureSetLoader.getImageAssetId(2));
                    outputMaterialData.m_textureSet.slicesFolders[2] = assetManager->getCombinedImageSlicesFolder(textureSetLoader.getImageAssetId(2));
                }
            }
        }
    }

    std::vector<bool> nodesVisited(model.nodes.size(), false);

    for (uint32_t nodeIdx = 0; nodeIdx < model.nodes.size(); nodeIdx++)
    {
        traverseNodes(outputData, model, nodeIdx, glm::mat4(1.0f), nodesVisited);
    }

    std::string cacheFilename = g_editorConfiguration->computeFullPathFromLocalPath(sceneLoadingInfo.filename + ".bin");
    ExternalSceneLoader::writeCache(cacheFilename, outputData);
}

void GLTFImporter::traverseNodes(ExternalSceneLoader::OutputData& outputData, const tinygltf::Model& model, uint32_t nodeIdx, const glm::mat4& parentTransform, std::vector<bool>& nodesVisited)
{
    if (nodesVisited[nodeIdx])
    {
        return;
    }
    nodesVisited[nodeIdx] = true;

    const tinygltf::Node& node = model.nodes[nodeIdx];
    glm::mat4 localTransform = glm::mat4(1.0f);

    if (node.matrix.size() == 16)
    {
        localTransform = glm::make_mat4(node.matrix.data());
    }
    else
    {
        if (node.translation.size() == 3)
            localTransform = glm::translate(localTransform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));

        if (node.rotation.size() == 4)
            localTransform *= glm::mat4_cast(glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]));

        if (node.scale.size() == 3)
            localTransform = glm::scale(localTransform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
    }

    glm::mat4 globalTransform = parentTransform * localTransform;

    if (node.mesh != -1)
    {
        Wolf::Debug::sendInfo("Found instance for mesh " + std::to_string(node.mesh));

        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (size_t primitiveIdx = 0; primitiveIdx < mesh.primitives.size(); primitiveIdx++)
        {
            const tinygltf::Primitive& primitive = mesh.primitives[primitiveIdx];
            uint32_t indexAccessorIdx = primitive.indices;

            if (std::ranges::find(m_skippedMeshes, indexAccessorIdx) != m_skippedMeshes.end())
            {
                Wolf::Debug::sendWarning("Instance use a skipped mesh");
                continue;
            }

            if (!m_meshesMap.contains(indexAccessorIdx))
            {
                Wolf::Debug::sendCriticalError("Mesh not found");
            }

            ExternalSceneLoader::InstanceData instanceData{};
            instanceData.m_meshIdx = m_meshesMap[indexAccessorIdx].m_outputMeshIdx;
            instanceData.m_transform = globalTransform;

            ImportedMeshInfo::MaterialIdentifier materialIdentifier(node.mesh, primitiveIdx);
            instanceData.m_materialIdx = m_meshesMap[indexAccessorIdx].m_materialsMap[materialIdentifier];

            outputData.m_instancesData.push_back(instanceData);
        }
    }

    for (int childIdx : node.children)
    {
        traverseNodes(outputData, model, childIdx, globalTransform, nodesVisited);
    }
}

uint32_t GLTFImporter::getAlbedoTextureIndex(tinygltf::Material& material)
{
    int albedoTextureIndex = -1;
    auto it = material.values.find("baseColorTexture");
    if (it != material.values.end())
    {
        albedoTextureIndex = it->second.TextureIndex();
    }
    else if (material.pbrMetallicRoughness.baseColorTexture.index != -1)
    {
        albedoTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
    }

    return static_cast<uint32_t>(albedoTextureIndex);
}

uint32_t GLTFImporter::getNormalTextureIndex(tinygltf::Material& material)
{
    int normalTextureIndex = -1;

    auto it = material.additionalValues.find("normalTexture");
    if (it != material.additionalValues.end())
    {
        normalTextureIndex = it->second.TextureIndex();
    }
    else if (material.normalTexture.index != -1)
    {
        normalTextureIndex = material.normalTexture.index;
    }

    return static_cast<uint32_t>(normalTextureIndex);
}

uint32_t GLTFImporter::getRoughnessTextureIndex(tinygltf::Material& material)
{
    auto it = material.values.find("roughnessTexture");
    if (it != material.values.end())
        return it->second.TextureIndex();

    return material.pbrMetallicRoughness.metallicRoughnessTexture.index;
}

uint32_t GLTFImporter::getMetallicTextureIndex(tinygltf::Material& material)
{
    auto it = material.values.find("metallicTexture");
    if (it != material.values.end())
        return it->second.TextureIndex();

    return material.pbrMetallicRoughness.metallicRoughnessTexture.index;
}

std::string GLTFImporter::getTextureURI(const tinygltf::Model& model, uint32_t textureIdx)
{
    if (textureIdx == -1 || textureIdx >= model.textures.size())
        return "";

    const tinygltf::Texture& texture = model.textures[textureIdx];
    const tinygltf::Image& image = model.images[texture.source];
    return image.uri;
}
