#pragma once

#include <string>

#include <AABB.h>
#include <BoundingSphere.h>
#include <PhysicShapes.h>

#include "TextureSetLoader.h"
#include "Vertex3D.h"

class AssetManager;

class ExternalSceneLoader
{
public:
    struct SceneLoadingInfo
    {
        // Files paths
        std::string filename;

        // Material options
        TextureSetLoader::InputTextureSetLayout textureSetLayout = TextureSetLoader::InputTextureSetLayout::EACH_TEXTURE_A_FILE;

        // Cache options
        bool useCache = true;

        // LODs
        uint32_t generateDefaultLODCount = 1;
        uint32_t generateSloppyLODCount = 1;
    };

    struct VertexData
    {
        std::vector<Vertex3D> m_staticVertices;
    };

    struct MeshData
    {
        std::string m_name;

        uint32_t m_vertexOffset;
        std::vector<uint32_t> m_indices;
        Wolf::AABB m_aabb;
        Wolf::BoundingSphere m_boundingSphere;

        // LOD data
        struct LODInfo
        {
            float m_error;
            uint32_t m_indexCount;
        };

        std::vector<std::vector<uint32_t>> m_defaultSimplifiedIndices;
        std::vector<LODInfo> m_defaultLODsInfo;
        std::vector<std::vector<uint32_t>> m_sloppySimplifiedIndices;
        std::vector<LODInfo> m_sloppyLODsInfo;

        std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>> m_physicsShapes;
    };

    struct MaterialData
    {
        Wolf::MaterialsGPUManager::TextureSetInfo m_textureSet;

        TextureSetLoader::TextureSetFileInfoGGX m_textureSetFileInfo;
    };

    struct InstanceData
    {
        glm::mat4 m_transform;
        uint32_t m_meshIdx;
        uint32_t m_materialIdx;
    };

    struct OutputData
    {
        VertexData m_vertexData;

        std::vector<MeshData> m_meshesData;
        std::vector<MaterialData> m_materialsData;
        std::vector<InstanceData> m_instancesData;
    };

    static void loadScene(OutputData& outputData, SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager);

    template<typename T>
    static void writeVector(std::ofstream& file, const std::vector<T>& vec)
    {
        uint32_t size = static_cast<uint32_t>(vec.size());
        file.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        if (size > 0)
        {
            file.write(reinterpret_cast<const char*>(vec.data()), size * sizeof(T));
        }
    }

    static void writeString(std::ofstream& file, const std::string& str)
    {
        uint32_t size = static_cast<uint32_t>(str.size());
        file.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        file.write(str.data(), size);
    }

    static void writeLODData(std::ofstream& file, const std::vector<std::vector<uint32_t>>& allIndices, const std::vector<MeshData::LODInfo>& info)
    {
        writeVector(file, info);

        uint32_t lodCount = static_cast<uint32_t>(allIndices.size());
        file.write(reinterpret_cast<const char*>(&lodCount), sizeof(uint32_t));
        for (const std::vector<uint32_t>& indices : allIndices)
        {
            writeVector(file, indices);
        }
    }
    static void writeCache(const std::string& filename, const OutputData& data);

private:
    template<typename T>
    static void readVector(std::ifstream& file, std::vector<T>& vec)
    {
        uint32_t size = 0;
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        vec.resize(size);
        if (size > 0)
        {
            file.read(reinterpret_cast<char*>(vec.data()), size * sizeof(T));
        }
    }

    static void readString(std::ifstream& file, std::string& str)
    {
        uint32_t size = 0;
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        str.resize(size);
        if (size > 0)
        {
            file.read(&str[0], size);
        }
    }

    static void readLODData(std::ifstream& file, std::vector<std::vector<uint32_t>>& allIndices, std::vector<MeshData::LODInfo>& info)
    {
        readVector(file, info);

        uint32_t lodCount = 0;
        file.read(reinterpret_cast<char*>(&lodCount), sizeof(uint32_t));
        allIndices.resize(lodCount);
        for (std::vector<uint32_t>& indices : allIndices)
        {
            readVector(file, indices);
        }
    }

    static void loadCache(const std::string& filename, OutputData& outData, AssetManager* assetManager);
};
