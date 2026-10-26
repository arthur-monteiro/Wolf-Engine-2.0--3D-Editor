#pragma once

#include <vector>

#include <AABB.h>
#include <BoundingSphere.h>
#include <MaterialsGPUManager.h>

#include "DAEImporter.h"
#include "InstanceMeshRenderer.h"
#include "TextureSetLoader.h"
#include "Vertex3D.h"

class AssetManager;

class MeshFormatter
{
public:
    static constexpr uint32_t MESHLET_MAX_VERTICES = 128;
    static constexpr uint32_t MESHLET_MAX_PRIMITIVES = 256;

    struct ReadSpecificLODInfo
    {
        uint32_t m_lod = -1;
        uint32_t m_lodType = -1;
    };
    MeshFormatter(const std::string& filename, AssetManager* assetManager, bool readLODData, ReadSpecificLODInfo readSpecificLOD);

    static std::string computeCacheFolder(const std::string& filename);
    static bool doesValidCacheExist(const std::string& filename);
    bool isMeshLoaded() const { return m_meshLoaded; }

    struct DataInput
    {
        std::string m_fileName;

        std::vector<Vertex3D> m_staticVertices;
        std::vector<SkeletonVertex> m_skeletonVertices;
        std::vector<uint32_t> m_indices;

        uint32_t m_generateDefaultLODCount = 1;
        uint32_t m_generateSloppyLODCount = 1;

        bool m_recomputeNormals = false;

        Wolf::ResourceUniqueOwner<AnimationData> m_animationData;
    };
    void computeData(const DataInput& input);

    uint32_t getIndexCount() const { return m_indexCount; }
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

    struct Meshlet
    {
        std::vector<Vertex3D> m_staticVertices;
        std::vector<uint8_t> m_indices;

        Wolf::AABB m_aabb;
        Wolf::BoundingSphere m_boundingSphere;
        uint32_t m_groupIdxAsParent = -1;
        uint32_t m_groupIdxAsChild = -1;
        Wolf::BoundingSphere m_parentGroupBoundingSphere;

        int8_t m_coneAxis[3];
        int8_t m_coneCutoff;

        std::vector<uint32_t> m_parentMeshletIndices;

        float m_lodError = 0.0f;
        float m_parentLodError = 1e30f;
    };
    const std::vector<Meshlet>& getMeshlets() { return m_meshlets; }

    struct MeshletGroup
    {
        Wolf::BoundingSphere m_boundingSphere;
        std::array<uint32_t, 4> m_childrenMeshletIndices;
        std::array<uint32_t, 4> m_parentMeshletIndices;

        uint32_t m_hierarchyLevel;
    };
    const std::vector<MeshletGroup>& getMeshletGroups() { return m_meshletGroups; }
    static void readMeshlet(Meshlet& outMeshlet, uint32_t meshletIdx, bool readMeshData, const std::string& cacheFolder);

    const std::vector<Wolf::MaterialsGPUManager::TextureSetInfo>& getTextureSetsInfo() const { return m_textureSetsInfo; }
    const Wolf::ResourceUniqueOwner<AnimationData>& getAnimationData() const { return m_animationData; }

private:
    AssetManager* m_assetManager = nullptr;

    void readLODs(bool readLODData, ReadSpecificLODInfo readSpecificLOD, uint32_t defaultLODCount, const std::vector<uint32_t>& indexCountPerDefaultLOD,
        const std::vector<uint32_t>& indexCountPerSloppyLOD);
    void buildMeshletHierarchy();
    void computeMeshlets(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices, std::vector<uint32_t>& outMeshletIndices, float currentLodError);
    void readMeshlets(bool readMeshData);

    template <typename T>
    void optimizeMeshData(std::vector<T>& outputVertices, std::vector<uint32_t>& outputIndices, const std::vector<T>& inputVertices, const std::vector<uint32_t>& inputIndices);

    template <typename T>
    void computeMeshInfo(std::vector<T>& vertices, const std::string& filename);

    template <typename T>
    void createLODs(std::vector<T>& vertices, uint32_t generateDefaultLODCount, uint32_t generateSloppyLODCount);

    static void writeBoneToCache(const AnimationData::Bone& bone, std::ofstream& file);
    static void readBoneFromCache(AnimationData::Bone& bone, std::ifstream& file);

    std::string m_cacheFolder;
    bool m_meshLoaded = false;

    std::vector<Vertex3D> m_staticVertices;
    std::vector<SkeletonVertex> m_skeletonVertices;
    uint32_t m_indexCount;
    std::vector<uint32_t> m_indices;

    // Meshlets
    std::vector<Meshlet> m_meshlets;
    std::vector<MeshletGroup> m_meshletGroups;

    struct MeshletGroupInternal
    {
        std::vector<uint32_t> m_meshletIndices;
    };
    std::vector<MeshletGroupInternal> computeMeshletGroups(const std::vector<uint32_t>& meshlets, uint32_t targetGroupSize);
    Wolf::BoundingSphere computeMeshletsForGroup(const MeshletGroupInternal& group, std::vector<uint32_t>& outMeshletIndices, bool isRootGroup);
    uint32_t computeMeshletCount(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices);

    bool m_isMeshCentered;
    Wolf::AABB m_aabb;
    Wolf::BoundingSphere m_boundingSphere;

    std::vector<LODInfo> m_defaultSimplifiedLODs;
    std::vector<LODInfo> m_sloppySimplifiedLODs;

    std::vector<Wolf::MaterialsGPUManager::TextureSetInfo> m_textureSetsInfo;
    Wolf::ResourceUniqueOwner<AnimationData> m_animationData;
};
