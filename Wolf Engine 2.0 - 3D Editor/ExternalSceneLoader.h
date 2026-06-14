#pragma once

#include <string>

#include <PhysicShapes.h>

#include "SkeletonVertex.h"
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
        std::vector<SkeletonVertex> m_skeletonVertices;
        std::vector<uint32_t> m_indices;

        Wolf::ResourceUniqueOwner<AnimationData> m_animationData;
        std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>> m_physicsShapes;

        // In case of very large files (Zorah), positions are stored in a file rather than RAM
        std::string m_cachePositionFilename;
        std::string m_cacheIndicesFilename;
    };

    struct MaterialData
    {
        TextureSetLoader::TextureSetFileInfoGGX m_textureSetFileInfo;
        glm::vec3 m_color = glm::vec3(1.0f);
    };

    struct InstanceData
    {
        glm::mat4 m_transform;
        uint32_t m_meshIdx;
        uint32_t m_materialIdx;
    };

    struct OutputData
    {
        bool m_hasReadMeshData;
        std::vector<MeshData> m_meshesData;
        std::vector<MaterialData> m_materialsData;
        std::vector<InstanceData> m_instancesData;
    };

    static void loadScene(OutputData& outputData, const SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager, bool readMeshData);

    static void writeCache(const std::string& filename, const OutputData& data);

private:
    static void loadCache(const std::string& filename, OutputData& outData, bool readMeshData);
    static void writeBoneToCache(std::ofstream& file, const AnimationData::Bone& bone);
    static void readBoneFromCache(std::ifstream& file, AnimationData::Bone& bone);
};
