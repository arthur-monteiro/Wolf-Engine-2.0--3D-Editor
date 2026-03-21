#pragma once

#include "ExternalSceneLoader.h"
#include "xxh64.hpp"

namespace tinygltf
{
    struct Material;
    class Node;
    class Model;
}

class AssetManager;

class GLTFImporter
{
public:
    GLTFImporter(ExternalSceneLoader::OutputData& outputData, const ExternalSceneLoader::SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager);

private:
    struct LoadedRange
    {
        struct Range
        {
            uint32_t m_min;
            uint32_t m_max;
        };
        std::array<Range, 4> m_bufferRanges;

        std::vector<Vertex3D> m_vertices;
        uint32_t m_indexBeforeSort;
        uint32_t m_outOffset = -1;
    };

    struct RangesInfoForMesh
    {
        struct RangeInfo
        {
            uint32_t m_rangeIdx;
            uint32_t m_offsetInPosRange;
        };

        std::vector<RangeInfo> m_ranges;
    };

    struct ImportedMeshInfo
    {
        uint32_t m_outputMeshIdx;

        struct MaterialIdentifier
        {
            uint32_t m_meshIdx;
            uint32_t m_primitiveIdx;

            MaterialIdentifier() : m_meshIdx(0), m_primitiveIdx(0) {}
            MaterialIdentifier(uint32_t meshIdx, uint32_t primitiveIdx) : m_meshIdx(meshIdx), m_primitiveIdx(primitiveIdx) {}

            bool operator==(const MaterialIdentifier& other) const
            {
                return m_meshIdx == other.m_meshIdx && m_primitiveIdx == other.m_primitiveIdx;
            }
        };
        struct MaterialIdentifierHasher
        {
            std::size_t operator()(const MaterialIdentifier& id) const noexcept
            {
                static_assert(sizeof(MaterialIdentifier) == 2 * sizeof(uint32_t)); // ensure that there no padding (otherwise hash will be incorrect)
                return xxh64::hash(reinterpret_cast<const char*>(&id), sizeof(MaterialIdentifier), 0);
            }
        };
        std::unordered_map<MaterialIdentifier, uint32_t /* output material idx */, MaterialIdentifierHasher> m_materialsMap;
    };
    std::unordered_map<uint32_t /* index of accessor for index buffer */, ImportedMeshInfo> m_meshesMap;
    std::vector<uint32_t /* index of accessor for index buffer */> m_skippedMeshes;

    void traverseNodes(ExternalSceneLoader::OutputData& outputData, const tinygltf::Model& model, uint32_t nodeIdx, const glm::mat4& parentTransform, std::vector<bool>& nodesVisited);

    static uint32_t getAlbedoTextureIndex(tinygltf::Material& material);
    static uint32_t getNormalTextureIndex(tinygltf::Material& material);
    static uint32_t getRoughnessTextureIndex(tinygltf::Material& material);
    static uint32_t getMetallicTextureIndex(tinygltf::Material& material);
    std::string getTextureURI(const tinygltf::Model& model, uint32_t textureIdx);
};
