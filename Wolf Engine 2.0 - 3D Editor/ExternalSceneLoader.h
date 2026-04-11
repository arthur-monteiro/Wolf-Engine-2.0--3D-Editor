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

    struct MeshData
    {
        std::string m_name;

        std::vector<Vertex3D> m_staticVertices;
        std::vector<uint32_t> m_indices;

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
        std::vector<MeshData> m_meshesData;
        std::vector<MaterialData> m_materialsData;
        std::vector<InstanceData> m_instancesData;
    };

    static void loadScene(OutputData& outputData, SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager);

    static void writeCache(const std::string& filename, const OutputData& data);

private:
    static void loadCache(const std::string& filename, OutputData& outData, AssetManager* assetManager);
};
