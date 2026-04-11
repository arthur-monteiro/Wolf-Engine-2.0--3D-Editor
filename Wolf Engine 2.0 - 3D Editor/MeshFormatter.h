#pragma once

#include <vector>

#include <AABB.h>
#include <BoundingSphere.h>
#include <MaterialsGPUManager.h>

#include "DAEImporter.h"
#include "TextureSetLoader.h"
#include "Vertex3D.h"

class AssetManager;

class MeshFormatter
{
public:
    MeshFormatter(const std::string& filename, AssetManager* assetManager);

    bool isMeshesLoaded() const { return m_meshLoaded; }

    struct DataInput
    {
        std::string m_fileName;

        std::vector<Vertex3D> m_staticVertices;
        std::vector<SkeletonVertex> m_skeletonVertices;
        std::vector<uint32_t> m_indices;

        uint32_t m_generateDefaultLODCount = 1;
        uint32_t m_generateSloppyLODCount = 1;

        std::vector<TextureSetLoader::TextureSetFileInfoGGX> m_textureSetsInfo;
        Wolf::ResourceUniqueOwner<AnimationData> m_animationData;
    };
    void computeData(const DataInput& input);

    const std::vector<Vertex3D>& getStaticVertices() { return m_staticVertices; }
    const std::vector<SkeletonVertex>& getSkeletonVertices() { return m_skeletonVertices; }
    const std::vector<uint32_t>& getIndices() { return m_indices; }

    bool isMeshCentered() const { return m_isMeshCentered; }
    const Wolf::AABB& getAABB() const { return m_aabb; }
    const Wolf::BoundingSphere& getBoundingSphere() const { return m_boundingSphere; }

    struct LODInfo
    {
        float m_error;
        uint32_t m_indexCount;

        std::vector<Vertex3D> m_staticVertices;
        std::vector<SkeletonVertex> m_skeletonVertices;
        std::vector<uint32_t> m_indices;

        LODInfo(float error, uint32_t indexCount, const std::vector<Vertex3D>& staticVertices, const std::vector<uint32_t>& indices)
            : m_error(error), m_indexCount(indexCount), m_staticVertices(staticVertices), m_indices(indices) {}
        LODInfo(float error, uint32_t indexCount, const std::vector<SkeletonVertex>& skeletonVertices, const std::vector<uint32_t>& indices)
            : m_error(error), m_indexCount(indexCount), m_skeletonVertices(skeletonVertices), m_indices(indices) {}
    };
    const std::vector<LODInfo>& getDefaultLODInfo() { return m_defaultSimplifiedLODs; }
    const std::vector<LODInfo>& getSloppyLODInfo() { return m_sloppySimplifiedLODs; }

    const std::vector<Wolf::MaterialsGPUManager::TextureSetInfo>& getTextureSetsInfo() const { return m_textureSetsInfo; }
    const Wolf::ResourceUniqueOwner<AnimationData>& getAnimationData() const { return m_animationData; }

private:
    AssetManager* m_assetManager = nullptr;

    template <typename T>
    void optimizeMeshData(std::vector<T>& outputVertices, std::vector<uint32_t>& outputIndices, const std::vector<T>& inputVertices, const std::vector<uint32_t>& inputIndices);

    template <typename T>
    void computeMeshInfo(std::vector<T>& vertices, const std::string& filename);

    template <typename T>
    void createLODs(std::vector<T>& vertices, uint32_t generateDefaultLODCount, uint32_t generateSloppyLODCount);

    void loadTextureSets(const std::vector<TextureSetLoader::TextureSetFileInfoGGX>& textureSetsFileInfo);
    static void writeBoneToCache(const AnimationData::Bone& bone, std::ofstream& file);
    static void readBoneFromCache(AnimationData::Bone& bone, std::ifstream& file);

    std::string m_cacheFilename;
    bool m_meshLoaded = false;

    std::vector<Vertex3D> m_staticVertices;
    std::vector<SkeletonVertex> m_skeletonVertices;
    std::vector<uint32_t> m_indices;

    bool m_isMeshCentered;
    Wolf::AABB m_aabb;
    Wolf::BoundingSphere m_boundingSphere;

    std::vector<LODInfo> m_defaultSimplifiedLODs;
    std::vector<LODInfo> m_sloppySimplifiedLODs;

    std::vector<Wolf::MaterialsGPUManager::TextureSetInfo> m_textureSetsInfo;
    Wolf::ResourceUniqueOwner<AnimationData> m_animationData;
};
