#pragma once

#include <tiny_obj_loader.h>

#include "DAEImporter.h"
#include "ImageCompression.h"
#include "TextureSetLoader.h"
#include "Mesh.h"
#include "PhysicShapes.h"
#include "ResourceUniqueOwner.h"
#include "Vertex3D.h"

class AssetManager;

struct ModelLoadingInfo
{
	// Files paths
	std::string filename;
	std::string mtlFolder;
#ifdef __ANDROID__
    bool isInAssets = false; // false = in storage
#endif

	// Material options
	TextureSetLoader::InputTextureSetLayout textureSetLayout = TextureSetLoader::InputTextureSetLayout::EACH_TEXTURE_A_FILE;

	// Cache options
	bool useCache = true;

	// LODs
	uint32_t generateDefaultLODCount = 1;
	uint32_t generateSloppyLODCount = 1;
};

struct ModelData
{
	std::vector<Vertex3D> m_staticVertices;
	std::vector<SkeletonVertex> m_skeletonVertices;

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

#ifdef MATERIAL_DEBUG
	std::string m_originFilepath;
#endif
	bool m_isMeshCentered;

	std::vector<Wolf::MaterialsGPUManager::TextureSetInfo> m_textureSets;

	Wolf::ResourceUniqueOwner<AnimationData> m_animationData;
	std::vector<Wolf::ResourceUniqueOwner<Wolf::Physics::Shape>> m_physicsShapes;
};

class ModelLoader
{
public:
	static void loadObject(ModelData& outputModel, ModelLoadingInfo& modelLoadingInfo, const Wolf::ResourceReference<AssetManager>& assetManager);

private:
	ModelLoader() = delete;
	ModelLoader(ModelData& outputModel, ModelLoadingInfo& modelLoadingInfo, const Wolf::ResourceReference<AssetManager>& assetManager);

	ModelData* m_outputModel;
	Wolf::ResourceReference<AssetManager> m_assetManager;

	struct InternalShapeInfo
	{
		uint32_t indicesOffset;
	};
	bool loadCache(ModelLoadingInfo& modelLoadingInfo);

	// Materials
	static TextureSetLoader::TextureSetFileInfoGGX createTextureSetFileInfoGGXFromTinyObjMaterial(const tinyobj::material_t& material, const std::string& mtlFolder);
	void loadTextureSet(const TextureSetLoader::TextureSetFileInfoGGX& material, uint32_t indexMaterial);

	float sRGBtoLinear(float component);

	bool m_useCache;
	struct InfoForCachePerTextureSet
	{
		TextureSetLoader::TextureSetFileInfoGGX m_textureSetFileInfoGGX;
	};
	std::vector<InfoForCachePerTextureSet> m_infoForCache;
};