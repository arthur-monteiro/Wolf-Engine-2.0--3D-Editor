#include "MeshFormatter.h"

#include <filesystem>
#include <fstream>
#include <meshoptimizer.h>

#include <ConfigurationHelper.h>

#include "AssetManager.h"
#include "CacheHelper.h"
#include "CodeFileHashes.h"
#include "EditorConfiguration.h"

template <typename T>
void MeshFormatter::optimizeMeshData(std::vector<T>& outputVertices, std::vector<uint32_t>& outputIndices, const std::vector<T>& inputVertices, const std::vector<uint32_t>& inputIndices)
{
	Wolf::Debug::sendInfo("Optimizing mesh...");

	std::vector<unsigned int> remap(inputVertices.size()); // temporary remap table
	size_t meshOptVertexCount = meshopt_generateVertexRemap(&remap[0], inputIndices.data(), inputIndices.size(),
		inputVertices.data(), inputVertices.size(), sizeof(T));
	std::vector<uint32_t> newIndices(inputIndices.size());
	meshopt_remapIndexBuffer(newIndices.data(), inputIndices.data(), inputIndices.size(), &remap[0]);
	std::vector<T> newVertices(meshOptVertexCount);
	meshopt_remapVertexBuffer(newVertices.data(), inputVertices.data(), inputVertices.size(), sizeof(T), &remap[0]);

	outputVertices = std::move(newVertices);
	outputIndices = std::move(newIndices);

	meshopt_optimizeVertexCache(outputIndices.data(), outputIndices.data(), outputIndices.size(), outputVertices.size());
}

template <typename T>
void MeshFormatter::computeMeshInfo(std::vector<T>& vertices, const std::string& filename)
{
	Wolf::Debug::sendInfo("Compute info...");

	glm::vec3 minPos(1'000'000.f);
	glm::vec3 maxPos(-1'000'000.f);

	for (T vertex : vertices)
	{
		if (vertex.pos.x < minPos.x)
			minPos.x = vertex.pos.x;
		if (vertex.pos.y < minPos.y)
			minPos.y = vertex.pos.y;
		if (vertex.pos.z < minPos.z)
			minPos.z = vertex.pos.z;

		if (vertex.pos.x > maxPos.x)
			maxPos.x = vertex.pos.x;
		if (vertex.pos.y > maxPos.y)
			maxPos.y = vertex.pos.y;
		if (vertex.pos.z > maxPos.z)
			maxPos.z = vertex.pos.z;
	}

	glm::vec3 center = (maxPos + minPos) * 0.5f;
	if (glm::length(center) > glm::length(maxPos) * 0.1f)
	{
		if (Wolf::ConfigurationHelper::readInfoFromFile(g_editorConfiguration->computeFullPathFromLocalPath(filename + ".config"), "forceCenter") == "true")
		{
			Wolf::Debug::sendInfo("Mesh is forced to be centered");

			for (T& vertex : vertices)
			{
				vertex.pos -= center;
			}

			maxPos -= center;
			minPos -= center;

			m_isMeshCentered = true;
		}
		else
		{
			Wolf::Debug::sendWarning("Model " + filename + " is not centered");
			m_isMeshCentered = false;
		}
	}
	else
	{
		m_isMeshCentered = true;
	}

	m_aabb = Wolf::AABB(minPos, maxPos);
	m_boundingSphere = Wolf::BoundingSphere(m_aabb);
}

template <typename T>
void MeshFormatter::createLODs(std::vector<T>& vertices, uint32_t generateDefaultLODCount, uint32_t generateSloppyLODCount)
{
    Wolf::Debug::sendInfo("Creating LODs...");

    auto processLOD = [&](uint32_t maxCount, bool isSloppy, auto& outStorage)
    {
        size_t targetIndexCount = m_indices.size();

        for (uint32_t i = 0; i < maxCount; ++i)
        {
	        constexpr float targetError = 1.0f;
	        if (targetIndexCount <= 16) break;
            targetIndexCount *= 0.5f;

            std::vector<uint32_t> lodIndices(m_indices.size());
            float lodError = 0.0f;

            size_t resultCount = 0;
        	static_assert(offsetof(T, pos) == 0);
            if (isSloppy)
            {
                resultCount = meshopt_simplifySloppy(lodIndices.data(), m_indices.data(), m_indices.size(),
                    reinterpret_cast<const float*>(vertices.data()), vertices.size(), sizeof(T), targetIndexCount, targetError, &lodError);
            }
        	else
        	{
                resultCount = meshopt_simplify(lodIndices.data(), m_indices.data(), m_indices.size(),
                    reinterpret_cast<const float*>(vertices.data()), vertices.size(), sizeof(T), targetIndexCount, targetError, 0, &lodError);
            }

            lodIndices.resize(resultCount);

            if (lodIndices.empty() || (!isSloppy && lodIndices.size() > targetIndexCount * 1.5f)) break;
            if (isSloppy && lodIndices.size() <= 16) break;

            std::vector<T> lodVertices;
            std::vector<uint32_t> newIndices;
            std::vector<int32_t> indexMap(vertices.size(), -1);

            lodVertices.reserve(lodIndices.size());
            newIndices.reserve(lodIndices.size());

            for (uint32_t oldIdx : lodIndices)
            {
                if (indexMap[oldIdx] == -1)
                {
                    indexMap[oldIdx] = static_cast<int32_t>(lodVertices.size());
                    lodVertices.push_back(vertices[oldIdx]);
                }
                newIndices.push_back(static_cast<uint32_t>(indexMap[oldIdx]));
            }

            outStorage.emplace_back(lodError, static_cast<uint32_t>(newIndices.size()), std::move(lodVertices), std::move(newIndices));
        }
    };

    processLOD(generateDefaultLODCount, false, m_defaultSimplifiedLODs);
    processLOD(generateSloppyLODCount, true, m_sloppySimplifiedLODs);
}

MeshFormatter::MeshFormatter(const std::string& filename, AssetManager* assetManager) : m_assetManager(assetManager)
{
	std::string escapedFilename = filename;
	for (size_t i = 0; i < escapedFilename.length(); ++i)
	{
		if (escapedFilename[i] == '/' || escapedFilename[i] == '\\')
		{
			escapedFilename[i] = '_';
		}
	}
	m_cacheFilename = g_editorConfiguration->getCacheFolderPath() + "/" + escapedFilename + ".bin";

	if (std::filesystem::exists(m_cacheFilename))
	{
		std::ifstream input(m_cacheFilename, std::ios::in | std::ios::binary);

		uint64_t hash;
		input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
		if (hash != Wolf::HASH_MESH_FORMATTER_CPP)
		{
			Wolf::Debug::sendInfo("Cache found but hash is incorrect");
			return;
		}

		input.read(reinterpret_cast<char*>(&m_isMeshCentered), sizeof(m_isMeshCentered));
		input.read(reinterpret_cast<char*>(&m_aabb), sizeof(m_aabb));
		input.read(reinterpret_cast<char*>(&m_boundingSphere), sizeof(m_boundingSphere));

		CacheHelper::readVector(input, m_staticVertices);
		CacheHelper::readVector(input, m_skeletonVertices);
		CacheHelper::readVector(input, m_indices);

		auto readLODStorage = [&](std::vector<LODInfo>& lods)
		{
			size_t count;
			input.read(reinterpret_cast<char*>(&count), sizeof(count));
			for (size_t i = 0; i < count; ++i)
			{
				float error;
				uint32_t indexCount;
				input.read(reinterpret_cast<char*>(&error), sizeof(error));
				input.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

				// Create a dummy LOD and then fill its vectors
				lods.emplace_back(error, indexCount, std::vector<Vertex3D>{}, std::vector<uint32_t>{});
				CacheHelper::readVector(input, lods.back().m_staticVertices);
				CacheHelper::readVector(input, lods.back().m_skeletonVertices);
				CacheHelper::readVector(input, lods.back().m_indices);
			}
		};

		readLODStorage(m_defaultSimplifiedLODs);
		readLODStorage(m_sloppySimplifiedLODs);

		size_t textureSetCount = 0;
		input.read(reinterpret_cast<char*>(&textureSetCount), sizeof(textureSetCount));
		std::vector<TextureSetLoader::TextureSetFileInfoGGX> textureSetsFileInfo(textureSetCount);

		for (TextureSetLoader::TextureSetFileInfoGGX& textureSetInfo : textureSetsFileInfo)
		{
			auto& [name, albedo, normal, roughness, metalness, ao, anisoStrength] = textureSetInfo;
			CacheHelper::readString(input, name);
			CacheHelper::readString(input, albedo);
			CacheHelper::readString(input, normal);
			CacheHelper::readString(input, roughness);
			CacheHelper::readString(input, metalness);
			CacheHelper::readString(input, ao);
			CacheHelper::readString(input, anisoStrength);
		}

		loadTextureSets(textureSetsFileInfo);

		bool hasAnimationData = false;
		input.read(reinterpret_cast<char*>(&hasAnimationData), sizeof(hasAnimationData));

		if (hasAnimationData)
		{
			m_animationData.reset(new AnimationData());

			input.read(reinterpret_cast<char*>(&m_animationData->m_boneCount), sizeof(uint32_t));

			uint32_t rootCount = 0;
			input.read(reinterpret_cast<char*>(&rootCount), sizeof(uint32_t));

			m_animationData->m_rootBones.resize(rootCount);
			for (uint32_t i = 0; i < rootCount; ++i)
			{
				readBoneFromCache(m_animationData->m_rootBones[i], input);
			}
		}

		m_meshLoaded = true;
	}
}

void MeshFormatter::computeData(const DataInput& input)
{
	if (!input.m_staticVertices.empty())
	{
		optimizeMeshData(m_staticVertices, m_indices, input.m_staticVertices, input.m_indices);
		computeMeshInfo(m_staticVertices, input.m_fileName);
		createLODs(m_staticVertices, input.m_generateDefaultLODCount, input.m_generateSloppyLODCount);
	}
	else if (!input.m_skeletonVertices.empty())
	{
		optimizeMeshData(m_skeletonVertices, m_indices, input.m_skeletonVertices, input.m_indices);
		computeMeshInfo(m_skeletonVertices, input.m_fileName);
		createLODs(m_skeletonVertices, input.m_generateDefaultLODCount, input.m_generateSloppyLODCount);
	}
	else
	{
		Wolf::Debug::sendCriticalError("No vertex provided");
	}

	loadTextureSets(input.m_textureSetsInfo);

	if (input.m_animationData)
	{
		m_animationData.reset(new AnimationData());
		*m_animationData = *input.m_animationData;
	}

	std::ofstream outCacheFile(m_cacheFilename, std::ios::binary);
	if (!outCacheFile.is_open())
	{
		Wolf::Debug::sendCriticalError("Can't open cache file for writing");
	}

	uint64_t hash = Wolf::HASH_MESH_FORMATTER_CPP;
	outCacheFile.write(reinterpret_cast<char*>(&hash), sizeof(hash));

	outCacheFile.write(reinterpret_cast<const char*>(&m_isMeshCentered), sizeof(m_isMeshCentered));
	outCacheFile.write(reinterpret_cast<const char*>(&m_aabb), sizeof(m_aabb));
	outCacheFile.write(reinterpret_cast<const char*>(&m_boundingSphere), sizeof(m_boundingSphere));

	CacheHelper::writeVector(outCacheFile, m_staticVertices);
	CacheHelper::writeVector(outCacheFile, m_skeletonVertices);
	CacheHelper::writeVector(outCacheFile, m_indices);

	auto writeLODStorage = [&](const std::vector<LODInfo>& lods)
	{
		size_t count = lods.size();
		outCacheFile.write(reinterpret_cast<const char*>(&count), sizeof(count));
		for (const LODInfo& lod : lods)
		{
			outCacheFile.write(reinterpret_cast<const char*>(&lod.m_error), sizeof(lod.m_error));
			outCacheFile.write(reinterpret_cast<const char*>(&lod.m_indexCount), sizeof(lod.m_indexCount));
			CacheHelper::writeVector(outCacheFile, lod.m_staticVertices);
			CacheHelper::writeVector(outCacheFile, lod.m_skeletonVertices);
			CacheHelper::writeVector(outCacheFile, lod.m_indices);
		}
	};

	writeLODStorage(m_defaultSimplifiedLODs);
	writeLODStorage(m_sloppySimplifiedLODs);

	// Texture sets info
	{
		size_t textureSetCount = input.m_textureSetsInfo.size();
		outCacheFile.write(reinterpret_cast<const char*>(&textureSetCount), sizeof(textureSetCount));
		for (const TextureSetLoader::TextureSetFileInfoGGX& textureSetInfo : input.m_textureSetsInfo)
		{
			const auto& [name, albedo, normal, roughness, metalness, ao, anisoStrength] = textureSetInfo;
			CacheHelper::writeString(outCacheFile, name);
			CacheHelper::writeString(outCacheFile, albedo);
			CacheHelper::writeString(outCacheFile, normal);
			CacheHelper::writeString(outCacheFile, roughness);
			CacheHelper::writeString(outCacheFile, metalness);
			CacheHelper::writeString(outCacheFile, ao);
			CacheHelper::writeString(outCacheFile, anisoStrength);
		}
	}

	// Animation data
	{
		bool hasAnimationData = static_cast<bool>(m_animationData);
		outCacheFile.write(reinterpret_cast<const char*>(&hasAnimationData), sizeof(hasAnimationData));

		if (hasAnimationData)
		{
			outCacheFile.write(reinterpret_cast<const char*>(&m_animationData->m_boneCount), sizeof(m_animationData->m_boneCount));
			uint32_t rootBoneCount = static_cast<uint32_t>(m_animationData->m_rootBones.size());
			outCacheFile.write(reinterpret_cast<const char*>(&rootBoneCount), sizeof(rootBoneCount));

			for (const AnimationData::Bone& root : m_animationData->m_rootBones)
			{
				writeBoneToCache(root, outCacheFile);
			}
		}
	}
}

void MeshFormatter::loadTextureSets(const std::vector<TextureSetLoader::TextureSetFileInfoGGX>& textureSetsFileInfo)
{
	m_textureSetsInfo.resize(textureSetsFileInfo.size());
	for (uint32_t i = 0; i < textureSetsFileInfo.size(); ++i)
	{
		const TextureSetLoader::TextureSetFileInfoGGX& textureSetFileInfo = textureSetsFileInfo[i];
		Wolf::MaterialsGPUManager::TextureSetInfo& textureSetInfo = m_textureSetsInfo[i];

		textureSetInfo.name = textureSetFileInfo.name;
		textureSetInfo.imageNames.resize(6);
		textureSetInfo.imageNames[0] = textureSetFileInfo.albedo;
		textureSetInfo.imageNames[1] = textureSetFileInfo.normal;
		textureSetInfo.imageNames[2] = textureSetFileInfo.roughness;
		textureSetInfo.imageNames[3] = textureSetFileInfo.metalness;
		textureSetInfo.imageNames[4] = textureSetFileInfo.ao;
		textureSetInfo.imageNames[5] = textureSetFileInfo.anisoStrength;

		TextureSetLoader::OutputLayout outputLayout;
		outputLayout.albedoCompression = Wolf::ImageCompression::Compression::BC1;
		outputLayout.normalCompression = Wolf::ImageCompression::Compression::BC5;

		TextureSetLoader textureSetLoader(textureSetFileInfo, outputLayout, true, m_assetManager);

		for (uint32_t i = 0; i < 2 /* albedo and normal */; ++i)
		{
			if (AssetId assetId = textureSetLoader.getImageAssetId(i); assetId != NO_ASSET)
			{
				textureSetInfo.imageAssetIds[i] = assetId;

				textureSetInfo.images[i] = m_assetManager->getImage(textureSetLoader.getImageAssetId(i));
				textureSetInfo.slicesFolders[i] = m_assetManager->getImageSlicesFolder(textureSetLoader.getImageAssetId(i));
			}
		}

		// Combined image
		if (AssetId assetId = textureSetLoader.getImageAssetId(2); assetId != NO_ASSET)
		{
			textureSetInfo.imageAssetIds[2] = assetId;

			textureSetInfo.images[2] = m_assetManager->getCombinedImage(textureSetLoader.getImageAssetId(2));
			textureSetInfo.slicesFolders[2] = m_assetManager->getCombinedImageSlicesFolder(textureSetLoader.getImageAssetId(2));
		}
	}
}

void MeshFormatter::writeBoneToCache(const AnimationData::Bone& bone, std::ofstream& file)
{
	file.write(reinterpret_cast<const char*>(&bone.m_idx), sizeof(bone.m_idx));
	CacheHelper::writeString(file, bone.m_name);
	file.write(reinterpret_cast<const char*>(&bone.m_offsetMatrix), sizeof(bone.m_offsetMatrix));

	CacheHelper::writeVector(file, bone.m_poses);

	uint32_t childrenCount = bone.m_children.size();
	file.write(reinterpret_cast<const char*>(&childrenCount), sizeof(childrenCount));
	for (const AnimationData::Bone& child : bone.m_children)
	{
		writeBoneToCache(child, file);
	}
}

void MeshFormatter::readBoneFromCache(AnimationData::Bone& bone, std::ifstream& file)
{
	file.read(reinterpret_cast<char*>(&bone.m_idx), sizeof(uint32_t));
	CacheHelper::readString(file, bone.m_name);
	file.read(reinterpret_cast<char*>(&bone.m_offsetMatrix), sizeof(glm::mat4));

	// Poses (Keyframes)
	CacheHelper::readVector(file, bone.m_poses);

	// Children (Recursion)
	uint32_t childCount = 0;
	file.read(reinterpret_cast<char*>(&childCount), sizeof(uint32_t));

	bone.m_children.resize(childCount);
	for (uint32_t i = 0; i < childCount; ++i)
	{
		readBoneFromCache(bone.m_children[i], file);
	}
}
