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

GLTFImporter::GLTFImporter(ExternalSceneLoader::OutputData& outputData, const ExternalSceneLoader::SceneLoadingInfo& sceneLoadingInfo, const Wolf::ResourceNonOwner<AssetManager>& assetManager)
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

    std::vector<std::vector<bool>> m_visitedBytesPerBuffer(model.buffers.size());
    for (uint32_t bufferIdx = 0; bufferIdx < model.buffers.size(); bufferIdx++)
    {
        m_visitedBytesPerBuffer[bufferIdx].resize(model.buffers[bufferIdx].data.size(), false);
    }

    std::vector<LoadedRange> loadedRanges;
    std::vector<RangesInfoForMesh> rangesPerMesh;
    std::array<uint32_t, 4> bufferIdxForEachInfo; // buffer idx for pos, norm, texCoords and tangent to check if it always the same
    bufferIdxForEachInfo.fill(-1);

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
                RangesInfoForMesh& rangeInfoForMesh = rangesPerMesh.emplace_back();
                meshData.m_name = (mesh.name.empty() ? "Mesh_" + std::to_string(meshIdx) : mesh.name) + "_prim_" + std::to_string(primitiveIdx);

                const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
                if (bufferIdxForEachInfo[0] == -1)
                {
                    bufferIdxForEachInfo[0] = posBufferView.buffer;
                }
                if (bufferIdxForEachInfo[0] != posBufferView.buffer)
                {
                    Wolf::Debug::sendCriticalError("All positions must be written in the same buffer");
                }
                const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];
                const uint32_t posByteStride = posBufferView.byteStride == 0 ? sizeof(float) * 3 : posBufferView.byteStride;
                if (posByteStride != sizeof(float) * 3)
                {
                    Wolf::Debug::sendCriticalError("We later assume that pos stride is 3 floats");
                }
                uint32_t firstUnvisitedPos = -1;
                uint32_t firstVisitedPos = -1;
                for (uint32_t posIdx = posAccessor.byteOffset + posBufferView.byteOffset; posIdx < posAccessor.byteOffset + posBufferView.byteOffset + posAccessor.count * posByteStride; posIdx++)
                {
                    if (firstUnvisitedPos == -1 && !m_visitedBytesPerBuffer[posBufferView.buffer][posIdx])
                    {
                        firstUnvisitedPos = posIdx;
                    }
                    else if (firstUnvisitedPos != -1 && firstVisitedPos == -1 && m_visitedBytesPerBuffer[posBufferView.buffer][posIdx])
                    {
                        firstVisitedPos = posIdx;
                    }
                    else if (firstVisitedPos != -1 && !m_visitedBytesPerBuffer[posBufferView.buffer][posIdx])
                    {
                        Wolf::Debug::sendCriticalError("We found a unvisited vertices, then visited vertices but again an unvisited vertex");
                    }
                }
                uint32_t posRangeMin = firstUnvisitedPos;
                uint32_t posRangeMax = firstVisitedPos != -1 ? firstVisitedPos : posAccessor.byteOffset + posBufferView.byteOffset + posAccessor.count * posByteStride;

                const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& normBufferView = model.bufferViews[normAccessor.bufferView];
                if (bufferIdxForEachInfo[1] == -1)
                {
                    bufferIdxForEachInfo[1] = normBufferView.buffer;
                }
                if (bufferIdxForEachInfo[1] != normBufferView.buffer)
                {
                    Wolf::Debug::sendCriticalError("All normals must be written in the same buffer");
                }
                const tinygltf::Buffer& normBuffer = model.buffers[normBufferView.buffer];
                const uint32_t normByteStride = normBufferView.byteStride == 0 ? sizeof(float) * 3 : normBufferView.byteStride;
                uint32_t firstUnvisitedNorm = -1;
                uint32_t firstVisitedNorm = -1;
                for (uint32_t normIdx = normAccessor.byteOffset + normBufferView.byteOffset; normIdx < normAccessor.byteOffset + normBufferView.byteOffset + normAccessor.count * normByteStride; normIdx++)
                {
                    if (firstUnvisitedNorm == -1 && !m_visitedBytesPerBuffer[normBufferView.buffer][normIdx])
                    {
                        firstUnvisitedNorm = normIdx;
                    }
                    else if (firstUnvisitedNorm != -1 && firstVisitedNorm == -1 && m_visitedBytesPerBuffer[normBufferView.buffer][normIdx])
                    {
                        firstVisitedNorm = normIdx;
                    }
                    else if (firstVisitedNorm != -1 && !m_visitedBytesPerBuffer[normBufferView.buffer][normIdx])
                    {
                        Wolf::Debug::sendCriticalError("We found a unvisited vertices, then visited vertices but again an unvisited vertex");
                    }
                }
                uint32_t normRangeMin = firstUnvisitedNorm;
                uint32_t normRangeMax = firstVisitedNorm != -1 ? firstVisitedNorm : normAccessor.byteOffset + normBufferView.byteOffset + normAccessor.count * normByteStride;

                const tinygltf::Accessor& texCoordsAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const tinygltf::BufferView& texCoordsBufferView = model.bufferViews[texCoordsAccessor.bufferView];
                const tinygltf::Buffer& texCoordsBuffer = model.buffers[texCoordsBufferView.buffer];
                const uint32_t texCoordsByteStride = texCoordsBufferView.byteStride == 0 ? sizeof(float) * 2 : texCoordsBufferView.byteStride;
                uint32_t firstUnvisitedTexCoords = -1;
                uint32_t firstVisitedTexCoords = -1;
                for (uint32_t texCoordsIdx = texCoordsAccessor.byteOffset + texCoordsBufferView.byteOffset; texCoordsIdx < texCoordsAccessor.byteOffset + texCoordsBufferView.byteOffset + texCoordsAccessor.count * texCoordsByteStride; texCoordsIdx++)
                {
                    if (firstUnvisitedTexCoords == -1 && !m_visitedBytesPerBuffer[texCoordsBufferView.buffer][texCoordsIdx])
                    {
                        firstUnvisitedTexCoords = texCoordsIdx;
                    }
                    else if (firstUnvisitedTexCoords != -1 && firstVisitedTexCoords == -1 && m_visitedBytesPerBuffer[texCoordsBufferView.buffer][texCoordsIdx])
                    {
                        firstVisitedTexCoords = texCoordsIdx;
                    }
                    else if (firstVisitedTexCoords != -1 && !m_visitedBytesPerBuffer[texCoordsBufferView.buffer][texCoordsIdx])
                    {
                        Wolf::Debug::sendCriticalError("We found a unvisited vertices, then visited vertices but again an unvisited vertex");
                    }
                }
                uint32_t texCoordsRangeMin = firstUnvisitedTexCoords;
                uint32_t texCoordsRangeMax = firstVisitedTexCoords != -1 ? firstVisitedTexCoords : texCoordsAccessor.byteOffset + texCoordsBufferView.byteOffset + texCoordsAccessor.count * texCoordsByteStride;

                const tinygltf::Accessor& tangentAccessor = model.accessors[primitive.attributes.at("TANGENT")];
                const tinygltf::BufferView& tangentBufferView = model.bufferViews[tangentAccessor.bufferView];
                const tinygltf::Buffer& tangentBuffer = model.buffers[tangentBufferView.buffer];
                const uint32_t tangentByteStride = tangentBufferView.byteStride == 0 ? sizeof(float) * 4 : tangentBufferView.byteStride;
                uint32_t firstUnvisitedTangent = -1;
                uint32_t firstVisitedTangent = -1;
                for (uint32_t tangentIdx = tangentAccessor.byteOffset + tangentBufferView.byteOffset; tangentIdx < tangentAccessor.byteOffset + tangentBufferView.byteOffset + tangentAccessor.count * tangentByteStride; tangentIdx++)
                {
                    if (firstUnvisitedTangent == -1 && !m_visitedBytesPerBuffer[tangentBufferView.buffer][tangentIdx])
                    {
                        firstUnvisitedTangent = tangentIdx;
                    }
                    else if (firstUnvisitedTangent != -1 && firstVisitedTangent == -1 && m_visitedBytesPerBuffer[tangentBufferView.buffer][tangentIdx])
                    {
                        firstVisitedTangent = tangentIdx;
                    }
                    else if (firstVisitedTangent != -1 && !m_visitedBytesPerBuffer[tangentBufferView.buffer][tangentIdx])
                    {
                        Wolf::Debug::sendCriticalError("We found a unvisited vertices, then visited vertices but again an unvisited vertex");
                    }
                }
                uint32_t tangentRangeMin = firstUnvisitedTangent;
                uint32_t tangentRangeMax = firstVisitedTangent != -1 ? firstVisitedTangent : tangentAccessor.byteOffset + tangentBufferView.byteOffset + tangentAccessor.count * tangentByteStride;


                if (normAccessor.count != posAccessor.count || normAccessor.count != texCoordsAccessor.count || normAccessor.count != tangentAccessor.count)
                {
                    Wolf::Debug::sendCriticalError("Error when reading GLTF file");
                }

                if (posRangeMin == -1)
                {
                    // Find ranges
                    uint32_t currentIndexToLookFor = posAccessor.byteOffset + posBufferView.byteOffset;
                    while (currentIndexToLookFor < posAccessor.byteOffset + posBufferView.byteOffset + posAccessor.count * posByteStride)
                    {
                        bool rangeFound = false;
                        for (uint32_t loadedRangeIdx = 0; loadedRangeIdx < loadedRanges.size(); loadedRangeIdx++)
                        {
                            const LoadedRange& loadedRange = loadedRanges[loadedRangeIdx];
                            if (loadedRange.m_bufferRanges[0].m_min <= currentIndexToLookFor && loadedRange.m_bufferRanges[0].m_max >= currentIndexToLookFor)
                            {
                                rangeInfoForMesh.m_ranges.push_back({ loadedRangeIdx, currentIndexToLookFor - loadedRange.m_bufferRanges[0].m_min });
                                currentIndexToLookFor = loadedRange.m_bufferRanges[0].m_max;

                                rangeFound = true;
                                break;
                            }
                        }

                        if (!rangeFound)
                        {
                            Wolf::Debug::sendCriticalError("No range found, it will infinitely loop");
                        }
                    }

                    continue; // we saw all vertices
                }

                if (normRangeMin == -1)
                {
                    Wolf::Debug::sendCriticalError("Range min shouldn't be -1 if pos isn't");
                }

                uint32_t posRangeCount = posRangeMax - posRangeMin;
                uint32_t normRangeCount = normRangeMax - normRangeMin;
                if (posRangeCount != normRangeCount)
                {
                    Wolf::Debug::sendCriticalError("Ranges should be the same for position and normal");
                }

                LoadedRange& addedLoadedRange = loadedRanges.emplace_back();
                addedLoadedRange.m_indexBeforeSort = loadedRanges.size() - 1;
                addedLoadedRange.m_bufferRanges[0].m_min = posRangeMin;
                addedLoadedRange.m_bufferRanges[0].m_max = posRangeMax;
                addedLoadedRange.m_bufferRanges[1].m_min = normRangeMin;
                addedLoadedRange.m_bufferRanges[1].m_max = normRangeMax;

                addedLoadedRange.m_vertices.resize(posRangeCount / posByteStride);

                for (uint32_t posIdx = posRangeMin; posIdx < posRangeMax; posIdx++)
                {
                    if ((posIdx - posRangeMin) % posByteStride == 0)
                    {
                        const float* posPtr = reinterpret_cast<const float*>(&(posBuffer.data[posIdx]));
                        addedLoadedRange.m_vertices[(posIdx - posRangeMin) / posByteStride].pos = glm::make_vec3(posPtr);
                    }
                    m_visitedBytesPerBuffer[posBufferView.buffer][posIdx] = true;
                }

                for (uint32_t normIdx = normRangeMin; normIdx < normRangeMax; normIdx++)
                {
                    if ((normIdx - normRangeMin) % normByteStride == 0)
                    {
                        const float* normPtr = reinterpret_cast<const float*>(&(normBuffer.data[normIdx]));
                        addedLoadedRange.m_vertices[(normIdx - normRangeMin) / normByteStride].normal = glm::make_vec3(normPtr);
                    }
                    m_visitedBytesPerBuffer[normBufferView.buffer][normIdx] = true;
                }

                for (uint32_t texCoordsIdx = texCoordsRangeMin; texCoordsIdx < texCoordsRangeMax; texCoordsIdx++)
                {
                    if ((texCoordsIdx - texCoordsRangeMin) % texCoordsByteStride == 0)
                    {
                        const float* texCoordsPtr = reinterpret_cast<const float*>(&(texCoordsBuffer.data[texCoordsIdx]));
                        addedLoadedRange.m_vertices[(texCoordsIdx - texCoordsRangeMin) / texCoordsByteStride].texCoord = glm::make_vec2(texCoordsPtr);
                    }
                    m_visitedBytesPerBuffer[texCoordsBufferView.buffer][texCoordsIdx] = true;
                }

                for (uint32_t tangentIdx = tangentRangeMin; tangentIdx < tangentRangeMax; tangentIdx++)
                {
                    if ((tangentIdx - tangentRangeMin) % tangentByteStride == 0)
                    {
                        const float* tangentsPtr = reinterpret_cast<const float*>(&(tangentBuffer.data[tangentIdx]));
                        addedLoadedRange.m_vertices[(tangentIdx - tangentRangeMin) / tangentByteStride].tangent = glm::make_vec3(tangentsPtr);
                    }
                    m_visitedBytesPerBuffer[texCoordsBufferView.buffer][tangentIdx] = true;
                }

                // Find ranges
                uint32_t currentIndexToLookFor = posAccessor.byteOffset + posBufferView.byteOffset;
                while (currentIndexToLookFor < posAccessor.byteOffset + posBufferView.byteOffset + posAccessor.count * posByteStride)
                {
                    bool rangeFound = false;
                    for (uint32_t loadedRangeIdx = 0; loadedRangeIdx < loadedRanges.size(); loadedRangeIdx++)
                    {
                        const LoadedRange& loadedRange = loadedRanges[loadedRangeIdx];
                        if (loadedRange.m_bufferRanges[0].m_min <= currentIndexToLookFor && loadedRange.m_bufferRanges[0].m_max >= currentIndexToLookFor)
                        {
                            rangeInfoForMesh.m_ranges.push_back({ loadedRangeIdx, currentIndexToLookFor - loadedRange.m_bufferRanges[0].m_min });
                            currentIndexToLookFor = loadedRange.m_bufferRanges[0].m_max;

                            rangeFound = true;
                            break;
                        }
                    }

                    if (!rangeFound)
                    {
                        Wolf::Debug::sendCriticalError("No range found, it will infinitely loop");
                    }
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

                if (posAccessor.minValues.size() == 3 && posAccessor.maxValues.size() == 3)
                {
                    glm::vec3 aabbMin, aabbMax;
                    aabbMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
                    aabbMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

                    meshData.m_aabb = Wolf::AABB(aabbMin, aabbMax);
                    meshData.m_boundingSphere = Wolf::BoundingSphere(meshData.m_aabb);
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

                TextureSetLoader::TextureSetFileInfoGGX materialFileInfoGGX{};
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

    Wolf::Debug::sendInfo("Sorting ranges");
    std::sort(loadedRanges.begin(), loadedRanges.end(),  [](const LoadedRange& a, const LoadedRange& b) { return a.m_bufferRanges[0].m_min < b.m_bufferRanges[0].m_min; });

    for (uint32_t loadedRangeIdx = 0; loadedRangeIdx < loadedRanges.size(); loadedRangeIdx++)
    {
        Wolf::Debug::sendInfo("Push range " + std::to_string(loadedRangeIdx) + " / " + std::to_string(loadedRanges.size()));

        LoadedRange& loadedRange = loadedRanges[loadedRangeIdx];

        uint32_t outOffset = outputData.m_vertexData.m_staticVertices.size();
        outputData.m_vertexData.m_staticVertices.resize(outputData.m_vertexData.m_staticVertices.size() + loadedRange.m_vertices.size());
        memcpy(&outputData.m_vertexData.m_staticVertices[outOffset], loadedRange.m_vertices.data(), loadedRange.m_vertices.size() * sizeof(loadedRange.m_vertices[0]));

        loadedRange.m_outOffset = outOffset;
    }

    for (uint32_t meshIdx = 0; meshIdx < outputData.m_meshesData.size(); ++meshIdx)
    {
        Wolf::Debug::sendInfo("Finding vertex offset for mesh " + std::to_string(meshIdx) + " / " + std::to_string(outputData.m_meshesData.size()));

        ExternalSceneLoader::MeshData& outputMeshData = outputData.m_meshesData[meshIdx];
        RangesInfoForMesh rangesInfoForMesh = rangesPerMesh[meshIdx];
        uint32_t firstRangeIdx = rangesInfoForMesh.m_ranges[0].m_rangeIdx;
        uint32_t firstRangeOffset = rangesInfoForMesh.m_ranges[0].m_offsetInPosRange / (3 * sizeof(float));

        bool rangeFound = false;
        for (const LoadedRange& loadedRange : loadedRanges)
        {
            if (loadedRange.m_indexBeforeSort == firstRangeIdx)
            {
                outputMeshData.m_vertexOffset = loadedRange.m_outOffset + firstRangeOffset;
                rangeFound = true;
                break;
            }
        }

        if (!rangeFound)
        {
            Wolf::Debug::sendCriticalError("Range not found");
        }
    }

    std::vector<bool> nodesVisited(model.nodes.size(), false);

    for (uint32_t nodeIdx = 0; nodeIdx < model.nodes.size(); nodeIdx++)
    {
        traverseNodes(outputData, model, nodeIdx, glm::mat4(1.0f), nodesVisited);
    }
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
