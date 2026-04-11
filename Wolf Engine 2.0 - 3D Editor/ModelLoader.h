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
};

struct ModelData
{
	std::vector<Vertex3D> m_staticVertices;
	std::vector<SkeletonVertex> m_skeletonVertices;

	std::vector<uint32_t> m_indices;

#ifdef MATERIAL_DEBUG
	std::string m_originFilepath;
#endif

	std::vector<TextureSetLoader::TextureSetFileInfoGGX> m_textureSets;

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

	// Materials
	static TextureSetLoader::TextureSetFileInfoGGX createTextureSetFileInfoGGXFromTinyObjMaterial(const tinyobj::material_t& material, const std::string& mtlFolder);

	float sRGBtoLinear(float component);
};