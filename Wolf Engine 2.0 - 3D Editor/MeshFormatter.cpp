#include "MeshFormatter.h"

#include <filesystem>
#include <fstream>
#include <meshoptimizer.h>
#include <ranges>

#include <ConfigurationHelper.h>

#include "AssetManager.h"
#include "CacheHelper.h"
#include "CodeFileHashes.h"
#include "EditorConfiguration.h"
#include "MathsUtilsEditor.h"

template <typename T>
void MeshFormatter::optimizeMeshData(std::vector<T>& outputVertices, std::vector<uint32_t>& outputIndices, const std::vector<T>& inputVertices, const std::vector<uint32_t>& inputIndices)
{
	Wolf::Debug::sendInfo("Optimizing mesh...");
	Wolf::Debug::sendInfo("Input contains " + std::to_string(inputIndices.size()) + " indices and " + std::to_string(inputVertices.size()) + " vertices");

	outputIndices = inputIndices;

	std::vector<unsigned int> remap(inputVertices.size()); // temporary remap table
	size_t meshOptVertexCount = meshopt_generateVertexRemap(&remap[0], outputIndices.data(), outputIndices.size(),
		inputVertices.data(), inputVertices.size(), sizeof(T));
	std::vector<uint32_t> newIndices(outputIndices.size());
	meshopt_remapIndexBuffer(newIndices.data(), outputIndices.data(), outputIndices.size(), &remap[0]);
	std::vector<T> newVertices(meshOptVertexCount);
	meshopt_remapVertexBuffer(newVertices.data(), inputVertices.data(), inputVertices.size(), sizeof(T), &remap[0]);

	outputVertices = std::move(newVertices);
	outputIndices = std::move(newIndices);

	meshopt_optimizeVertexCache(outputIndices.data(), outputIndices.data(), outputIndices.size(), outputVertices.size());

	Wolf::Debug::sendInfo("Output contains " + std::to_string(outputIndices.size()) + " indices and " + std::to_string(outputVertices.size()) + " vertices");
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

MeshFormatter::MeshFormatter(const std::string& filename, AssetManager* assetManager, bool readLODData, ReadSpecificLODInfo readSpecificLOD) : m_assetManager(assetManager)
{
	m_cacheFolder = computeCacheFolder(filename);
	std::string meshInfoFilepath = m_cacheFolder + "meshInfo.bin";

	if (std::filesystem::exists(meshInfoFilepath))
	{
		std::ifstream meshInfoInputFile(meshInfoFilepath, std::ios::in | std::ios::binary);

		uint64_t hash;
		meshInfoInputFile.read(reinterpret_cast<char*>(&hash), sizeof(hash));
		if (hash != Wolf::HASH_MESH_FORMATTER_CPP)	
		{
			Wolf::Debug::sendInfo("Cache found but hash is incorrect");
			return;
		}

		meshInfoInputFile.read(reinterpret_cast<char*>(&m_isMeshCentered), sizeof(m_isMeshCentered));
		meshInfoInputFile.read(reinterpret_cast<char*>(&m_aabb), sizeof(m_aabb));
		meshInfoInputFile.read(reinterpret_cast<char*>(&m_boundingSphere), sizeof(m_boundingSphere));

		meshInfoInputFile.read(reinterpret_cast<char*>(&m_indexCount), sizeof(uint32_t));

		uint32_t defaultLODCount;
		meshInfoInputFile.read(reinterpret_cast<char*>(&defaultLODCount), sizeof(uint32_t));

		std::vector<uint32_t> indexCountPerDefaultLOD;
		CacheHelper::readVector(meshInfoInputFile, indexCountPerDefaultLOD);

		uint32_t sloppyLODCount;
		meshInfoInputFile.read(reinterpret_cast<char*>(&sloppyLODCount), sizeof(uint32_t));

		std::vector<uint32_t> indexCountPerSloppyLOD;
		CacheHelper::readVector(meshInfoInputFile, indexCountPerSloppyLOD);

		uint32_t meshletCount;
		meshInfoInputFile.read(reinterpret_cast<char*>(&meshletCount), sizeof(uint32_t));

		meshInfoInputFile.close();

		if (!Wolf::g_configuration->getUseMeshlets())
		{
			readLODs(readLODData, readSpecificLOD, defaultLODCount, indexCountPerDefaultLOD, indexCountPerSloppyLOD);
		}
		else
		{
			m_meshlets.resize(meshletCount);
		 	readMeshlets();
		}

		size_t textureSetCount = 0;
		meshInfoInputFile.read(reinterpret_cast<char*>(&textureSetCount), sizeof(textureSetCount));
		std::vector<TextureSetLoader::TextureSetFileInfoGGX> textureSetsFileInfo(textureSetCount);

		for (TextureSetLoader::TextureSetFileInfoGGX& textureSetInfo : textureSetsFileInfo)
		{
			auto& [name, albedo, normal, roughness, metalness, ao, anisoStrength] = textureSetInfo;
			CacheHelper::readString(meshInfoInputFile, name);
			CacheHelper::readString(meshInfoInputFile, albedo);
			CacheHelper::readString(meshInfoInputFile, normal);
			CacheHelper::readString(meshInfoInputFile, roughness);
			CacheHelper::readString(meshInfoInputFile, metalness);
			CacheHelper::readString(meshInfoInputFile, ao);
			CacheHelper::readString(meshInfoInputFile, anisoStrength);
		}

		bool hasAnimationData = false;
		meshInfoInputFile.read(reinterpret_cast<char*>(&hasAnimationData), sizeof(hasAnimationData));

		if (hasAnimationData)
		{
			m_animationData.reset(new AnimationData());

			meshInfoInputFile.read(reinterpret_cast<char*>(&m_animationData->m_boneCount), sizeof(uint32_t));

			uint32_t rootCount = 0;
			meshInfoInputFile.read(reinterpret_cast<char*>(&rootCount), sizeof(uint32_t));

			m_animationData->m_rootBones.resize(rootCount);
			for (uint32_t i = 0; i < rootCount; ++i)
			{
				readBoneFromCache(m_animationData->m_rootBones[i], meshInfoInputFile);
			}
		}

		m_meshLoaded = true;
	}
}

bool MeshFormatter::doesValidCacheExist(const std::string& filename)
{
	std::string cacheFilename = computeCacheFolder(filename);
	if (std::filesystem::exists(cacheFilename))
	{
		std::ifstream input(cacheFilename, std::ios::in | std::ios::binary);

		uint64_t hash;
		input.read(reinterpret_cast<char*>(&hash), sizeof(hash));
		if (hash != Wolf::HASH_MESH_FORMATTER_CPP)
		{
			return false;
		}
		return true;
	}
	return false;
}

void MeshFormatter::computeData(const DataInput& input)
{
	if (!input.m_staticVertices.empty())
	{
		optimizeMeshData(m_staticVertices, m_indices, input.m_staticVertices, input.m_indices);
		if (input.m_recomputeNormals)
		{
			computeNormals(m_staticVertices, m_indices);
		}
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

	m_indexCount = m_indices.size();

	if (input.m_animationData)
	{
		m_animationData.reset(new AnimationData());
		*m_animationData = *input.m_animationData;
	}

	if (!std::filesystem::is_directory(m_cacheFolder) || !std::filesystem::exists(m_cacheFolder))
	{
		std::filesystem::create_directory(m_cacheFolder);
	}

	std::string meshInfoFilepath = m_cacheFolder + "meshInfo.bin";
	std::ofstream outMeshInfoFile(meshInfoFilepath, std::ios::binary);
	if (!outMeshInfoFile.is_open())
	{
		Wolf::Debug::sendCriticalError("Can't open cache file for writing");
	}

	uint64_t hash = Wolf::HASH_MESH_FORMATTER_CPP;
	outMeshInfoFile.write(reinterpret_cast<char*>(&hash), sizeof(hash));

	outMeshInfoFile.write(reinterpret_cast<const char*>(&m_isMeshCentered), sizeof(m_isMeshCentered));
	outMeshInfoFile.write(reinterpret_cast<const char*>(&m_aabb), sizeof(m_aabb));
	outMeshInfoFile.write(reinterpret_cast<const char*>(&m_boundingSphere), sizeof(m_boundingSphere));

	outMeshInfoFile.write(reinterpret_cast<const char*>(&m_indexCount), sizeof(uint32_t));

	uint32_t defaultLODCount = m_defaultSimplifiedLODs.size();
	outMeshInfoFile.write(reinterpret_cast<const char*>(&defaultLODCount), sizeof(uint32_t));

	std::vector<uint32_t> indexCountPerDefaultLOD(defaultLODCount);
	for (uint32_t i = 0; i < defaultLODCount; ++i)
	{
		indexCountPerDefaultLOD[i] = m_defaultSimplifiedLODs[i].m_indexCount;
	}
	CacheHelper::writeVector(outMeshInfoFile, indexCountPerDefaultLOD);

	uint32_t sloppyLODCount = m_sloppySimplifiedLODs.size();
	outMeshInfoFile.write(reinterpret_cast<const char*>(&sloppyLODCount), sizeof(uint32_t));

	std::vector<uint32_t> indexCountPerSloppyLOD(sloppyLODCount);
	for (uint32_t i = 0; i < sloppyLODCount; ++i)
	{
		indexCountPerSloppyLOD[i] = m_sloppySimplifiedLODs[i].m_indexCount;
	}
	CacheHelper::writeVector(outMeshInfoFile, indexCountPerSloppyLOD);

	auto writeBuffers = [this](std::ofstream& outFile, const std::vector<Vertex3D>& staticVertices, const std::vector<SkeletonVertex>& skeletonVertices, const std::vector<uint32_t>& indices)
	{
		{
			size_t vertexCount = staticVertices.size();
			outFile.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));

			size_t maxCompressedSize = meshopt_encodeVertexBufferBound(staticVertices.size(), sizeof(Vertex3D));
			std::vector<unsigned char> compressedVertices(maxCompressedSize);

			size_t compressedVerticesSize = meshopt_encodeVertexBuffer(compressedVertices.data(), compressedVertices.size(), staticVertices.data(), staticVertices.size(),
				sizeof(Vertex3D));
			compressedVertices.resize(compressedVerticesSize);
			CacheHelper::writeVector(outFile, compressedVertices);
		}

		{
			size_t vertexCount = skeletonVertices.size();
			outFile.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));

			size_t maxCompressedSize = meshopt_encodeVertexBufferBound(skeletonVertices.size(), sizeof(SkeletonVertex));
			std::vector<unsigned char> compressedVertices(maxCompressedSize);

			size_t compressedVerticesSize = meshopt_encodeVertexBuffer(compressedVertices.data(), compressedVertices.size(), skeletonVertices.data(), skeletonVertices.size(),
				sizeof(SkeletonVertex));
			compressedVertices.resize(compressedVerticesSize);
			CacheHelper::writeVector(outFile, compressedVertices);
		}

		{
			size_t indexCount = indices.size();
			outFile.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));

			size_t maxCompressedSize = meshopt_encodeIndexBufferBound(indices.size(), std::max(staticVertices.size(), skeletonVertices.size()));
			std::vector<unsigned char> compressedIndices(maxCompressedSize);

			size_t compressedIndicesSize = meshopt_encodeIndexBuffer(compressedIndices.data(), compressedIndices.size(), indices.data(), indices.size());
			compressedIndices.resize(compressedIndicesSize);
			CacheHelper::writeVector(outFile, compressedIndices);
		}
	};

	std::string bestLODFilepath = m_cacheFolder + "lod0.bin";
	std::ofstream bestLODCacheFile(bestLODFilepath, std::ios::binary);
	if (!bestLODCacheFile.is_open())
	{
		Wolf::Debug::sendCriticalError("Can't open cache file for writing");
	}

	writeBuffers(bestLODCacheFile, m_staticVertices, m_skeletonVertices, m_indices);

	bestLODCacheFile.close();

	auto writeLODStorage = [&](const std::vector<LODInfo>& lods, const std::string& prefix)
	{
		for (uint32_t lodIndex = 0; lodIndex < lods.size(); ++lodIndex)
		{
			const LODInfo& lod = lods[lodIndex];

			std::string lodFilepath = m_cacheFolder + prefix + "_lod" + std::to_string(lodIndex) + ".bin";
			std::ofstream lodCacheFile(lodFilepath, std::ios::binary);
			if (!lodCacheFile.is_open())
			{
				Wolf::Debug::sendCriticalError("Can't open cache file for writing");
			}

			lodCacheFile.write(reinterpret_cast<const char*>(&lod.m_error), sizeof(lod.m_error));
			writeBuffers(lodCacheFile, lod.m_staticVertices, lod.m_skeletonVertices, lod.m_indices);

			lodCacheFile.close();
		}
	};

	writeLODStorage(m_defaultSimplifiedLODs, "default");
	writeLODStorage(m_sloppySimplifiedLODs, "sloppy");

	// Meshlets
	if (input.m_skeletonVertices.empty())
	{
		buildMeshletHierarchy();
	}

	for (uint32_t meshletIdx = 0; meshletIdx < m_meshlets.size(); ++meshletIdx)
	{
		const Meshlet meshlet = m_meshlets[meshletIdx];

		std::string meshletFilepath = m_cacheFolder + "meshlet" + std::to_string(meshletIdx) + ".bin";
		std::ofstream meshletCacheFile(meshletFilepath, std::ios::binary);
		if (!meshletCacheFile.is_open())
		{
			Wolf::Debug::sendCriticalError("Can't open cache file for writing");
		}

		CacheHelper::writeVector(meshletCacheFile, meshlet.m_staticVertices);
		CacheHelper::writeVector(meshletCacheFile, meshlet.m_indices);

		meshletCacheFile.write(reinterpret_cast<const char*>(&meshlet.m_aabb), sizeof(Wolf::AABB));
		meshletCacheFile.write(reinterpret_cast<const char*>(&meshlet.m_boundingSphere), sizeof(Wolf::BoundingSphere));
		meshletCacheFile.write(reinterpret_cast<const char*>(&meshlet.m_groupBoundingSphere), sizeof(Wolf::BoundingSphere));
		meshletCacheFile.write(reinterpret_cast<const char*>(&meshlet.m_parentGroupBoundingSphere), sizeof(Wolf::BoundingSphere));
		meshletCacheFile.write(reinterpret_cast<const char*>(&meshlet.m_coneAxis), sizeof(uint8_t) * 3);
		meshletCacheFile.write(reinterpret_cast<const char*>(&meshlet.m_coneCutoff), sizeof(uint8_t));

		CacheHelper::writeVector(meshletCacheFile, meshlet.m_parentMeshletIndices);

		meshletCacheFile.write(reinterpret_cast<const char*>(&meshlet.m_lodError), sizeof(float));
		meshletCacheFile.write(reinterpret_cast<const char*>(&meshlet.m_parentLodError), sizeof(float));

		meshletCacheFile.close();
	}

	uint32_t meshletCount = m_meshlets.size();
	outMeshInfoFile.write(reinterpret_cast<const char*>(&meshletCount), sizeof(uint32_t));

	outMeshInfoFile.close();

	// Animation data
	{
		std::string animationCacheFilepath = m_cacheFolder + "animationData.bin";
		std::ofstream animationCacheFile(animationCacheFilepath, std::ios::binary);
		if (!animationCacheFile.is_open())
		{
			Wolf::Debug::sendCriticalError("Can't open cache file for writing");
		}

		bool hasAnimationData = static_cast<bool>(m_animationData);
		animationCacheFile.write(reinterpret_cast<const char*>(&hasAnimationData), sizeof(hasAnimationData));

		if (hasAnimationData)
		{
			animationCacheFile.write(reinterpret_cast<const char*>(&m_animationData->m_boneCount), sizeof(m_animationData->m_boneCount));
			uint32_t rootBoneCount = static_cast<uint32_t>(m_animationData->m_rootBones.size());
			animationCacheFile.write(reinterpret_cast<const char*>(&rootBoneCount), sizeof(rootBoneCount));

			for (const AnimationData::Bone& root : m_animationData->m_rootBones)
			{
				writeBoneToCache(root, animationCacheFile);
			}
		}

		animationCacheFile.close();
	}
}

std::string MeshFormatter::computeCacheFolder(const std::string& filename)
{
	std::string escapedFilename = filename;
	for (size_t i = 0; i < escapedFilename.length(); ++i)
	{
		if (escapedFilename[i] == '/' || escapedFilename[i] == '\\')
		{
			escapedFilename[i] = '_';
		}
	}
	return  g_editorConfiguration->getCacheFolderPath() + "/" + escapedFilename + "/";
}

void MeshFormatter::readLODs(bool readLODData, ReadSpecificLODInfo readSpecificLOD, uint32_t defaultLODCount, const std::vector<uint32_t>& indexCountPerDefaultLOD,
	const std::vector<uint32_t>& indexCountPerSloppyLOD)
{
	auto decompressAndReadBuffers = [&](std::ifstream& inputFile, std::vector<Vertex3D>& staticVertices, std::vector<SkeletonVertex>& skeletonVertices, std::vector<uint32_t>& indices)
		{
		    {
		    	size_t vertexCount = 0;
		    	inputFile.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));

		        std::vector<unsigned char> compressedVertices;
		        CacheHelper::readVector(inputFile, compressedVertices);

		        staticVertices.resize(vertexCount);
		        int result = meshopt_decodeVertexBuffer(
		            staticVertices.data(),
		            vertexCount,
		            sizeof(Vertex3D),
		            compressedVertices.data(),
		            compressedVertices.size()
		        );

		        if (result != 0)
		        {
		            Wolf::Debug::sendCriticalError("Failed to decompress vertex buffer");
		        }
		    }

		    {
		    	size_t vertexCount = 0;
		    	inputFile.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));

		        std::vector<unsigned char> compressedVertices;
		        CacheHelper::readVector(inputFile, compressedVertices);

		        skeletonVertices.resize(vertexCount);
		        int result = meshopt_decodeVertexBuffer(
		            skeletonVertices.data(),
		            vertexCount,
		            sizeof(SkeletonVertex),
		            compressedVertices.data(),
		            compressedVertices.size()
		        );

		        if (result != 0)
		        {
		        	Wolf::Debug::sendCriticalError("Failed to decompress vertex buffer");
		        }
		    }

		    {
		    	size_t indexCount = 0;
		    	inputFile.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

		        std::vector<unsigned char> compressedIndices;
		        CacheHelper::readVector(inputFile, compressedIndices);

		        indices.resize(indexCount);
		        int result = meshopt_decodeIndexBuffer(
		            indices.data(),
		            indexCount,
		            sizeof(uint32_t),
		            compressedIndices.data(),
		            compressedIndices.size()
		        );

		        if (result != 0)
		        {
		        	Wolf::Debug::sendCriticalError("Failed to decompress index buffer");
		        }
		    }
		};

	if (readLODData || defaultLODCount == 0 || readSpecificLOD.m_lod == 0)
	{
		std::string bestLODCacheFilepath = m_cacheFolder + "lod0.bin";
		std::ifstream bestLODCacheFile(bestLODCacheFilepath, std::ios::in | std::ios::binary);

		if (!bestLODCacheFile.good())
		{
			Wolf::Debug::sendCriticalError("Failed to open lod cache");
		}

		decompressAndReadBuffers(bestLODCacheFile, m_staticVertices, m_skeletonVertices, m_indices);

		bestLODCacheFile.close();

		if (m_indexCount != m_indices.size())
		{
			Wolf::Debug::sendCriticalError("Index count mismatch");
		}
	}

	bool worstLODToUseIsLastDefault = false;
	auto readLODStorage = [&](std::vector<LODInfo>& lods, uint32_t lodType, const std::string& prefix)
	{
		uint32_t count = 0;
		if (lodType == 0)
			count = indexCountPerDefaultLOD.size();
		else if (lodType == 1)
			count = indexCountPerSloppyLOD.size();
		else
			Wolf::Debug::sendCriticalError("Unsupported LOD type");

		for (size_t i = 0; i < count; ++i)
		{
			uint32_t indexCount = 0;
			if (lodType == 0)
				indexCount = indexCountPerDefaultLOD[i];
			else if (lodType == 1)
				indexCount = indexCountPerSloppyLOD[i];
			else
				Wolf::Debug::sendCriticalError("Unsupported LOD type");

			bool readThisLOD = readLODData;
			if (readSpecificLOD.m_lod - 1 == i && readSpecificLOD.m_lodType == lodType)
				readThisLOD = true;
			if (readSpecificLOD.m_lod == -1 && i == count - 1)
			{
				if (lodType == 0 && indexCount < 1024)
				{
					worstLODToUseIsLastDefault = true;
					readThisLOD = true;
				}
				else if (lodType == 1 && !worstLODToUseIsLastDefault)
				{
					readThisLOD = true;
				}
			}

			lods.emplace_back(-1, indexCount, std::vector<Vertex3D>{}, std::vector<uint32_t>{});

			if (readThisLOD)
			{
				std::string lodCacheFilepath = m_cacheFolder + prefix + "_lod" + std::to_string(i) + ".bin";
				std::ifstream lodCacheFile(lodCacheFilepath, std::ios::in | std::ios::binary);

				if (!lodCacheFile.good())
				{
					Wolf::Debug::sendCriticalError("Failed to open lod cache");
				}

				float error;
				lodCacheFile.read(reinterpret_cast<char*>(&error), sizeof(error));
				decompressAndReadBuffers(lodCacheFile, lods.back().m_staticVertices, lods.back().m_skeletonVertices, lods.back().m_indices);

				lodCacheFile.close();
			}
		}
	};

	readLODStorage(m_defaultSimplifiedLODs, 0, "default");
	readLODStorage(m_sloppySimplifiedLODs, 1, "sloppy");
}
void MeshFormatter::buildMeshletHierarchy()
{
    Wolf::Debug::sendInfo("Building meshlet hierarchy");

    m_meshlets.clear();

    std::vector<uint32_t> currentLevelMeshletIndices;
    computeMeshlets(m_staticVertices, m_indices, currentLevelMeshletIndices, 0.0f, m_boundingSphere);

    Wolf::Debug::sendInfo("Computed " + std::to_string(currentLevelMeshletIndices.size()) + " base meshlets");

    while (currentLevelMeshletIndices.size() > 1)
    {
	    constexpr uint32_t TARGET_GROUP_SIZE = 4;

	    Wolf::Debug::sendInfo("Merging to new level (" + std::to_string(currentLevelMeshletIndices.size()) + " meshlets remaining)");

        std::vector<MeshletGroup> meshletGroups;
        bool isRootPass = (currentLevelMeshletIndices.size() <= 2 * TARGET_GROUP_SIZE);

        if (isRootPass)
        {
            MeshletGroup rootGroup;
            rootGroup.m_meshletIndices = currentLevelMeshletIndices;
            meshletGroups.push_back(rootGroup);
        }
        else
        {
            meshletGroups = computeMeshletGroups(currentLevelMeshletIndices, TARGET_GROUP_SIZE);
        }

        if (meshletGroups.size() == currentLevelMeshletIndices.size() && !isRootPass)
        {
            Wolf::Debug::sendCriticalError("Meshlet grouping failed to form clusters");
        }

        std::vector<uint32_t> nextLevelMeshletIndices;

        for (const MeshletGroup& group : meshletGroups)
        {
            uint32_t previousSize = static_cast<uint32_t>(m_meshlets.size());

            std::vector<uint32_t> unusedLocalIndices;
            computeMeshletsForGroup(group, unusedLocalIndices, isRootPass);

            uint32_t currentSize = static_cast<uint32_t>(m_meshlets.size());

            std::vector<uint32_t> groupParentIndices;
            groupParentIndices.reserve(currentSize - previousSize);

            for (uint32_t idx = previousSize; idx < currentSize; ++idx)
            {
                groupParentIndices.push_back(idx);
                nextLevelMeshletIndices.push_back(idx);
            }

            for (uint32_t childIdx : group.m_meshletIndices)
            {
                Meshlet& childMeshlet = m_meshlets[childIdx];
                childMeshlet.m_parentMeshletIndices = groupParentIndices;
            }
        }

        if (isRootPass)
        {
            break;
        }

    	if (nextLevelMeshletIndices.size() == currentLevelMeshletIndices.size())
    	{
    		// TODO: ideally we should re-do simplification for this level as root pass (to also simplify edges)
    		Wolf::Debug::sendInfo("Computed can't reduce more");
    		break;
    	}

        currentLevelMeshletIndices = std::move(nextLevelMeshletIndices);
        Wolf::Debug::sendInfo("Computed " + std::to_string(currentLevelMeshletIndices.size()) + " meshlets");
    }

    Wolf::Debug::sendInfo("Meshlet hierarchy complete. Total nodes: " + std::to_string(m_meshlets.size()));
}

void MeshFormatter::computeMeshlets(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices, std::vector<uint32_t>& outMeshletIndices, float currentLodError,
	const Wolf::BoundingSphere& groupBoundingSphere)
{
	size_t maxVertices = 64;
	size_t maxTriangles = 124;
	float coneWeight = 0.0f;

	size_t maxMeshlets = meshopt_buildMeshletsBound(indices.size(), maxVertices, maxTriangles);
	std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
	std::vector<uint32_t> meshletVertices(maxMeshlets * maxVertices);
	std::vector<unsigned char> meshletTriangles(maxMeshlets * maxTriangles * 3);

	size_t meshletCount = meshopt_buildMeshlets(
		meshlets.data(),
		meshletVertices.data(),
		meshletTriangles.data(),
		indices.data(),
		indices.size(),
		&vertices[0].pos[0],
		vertices.size(),
		sizeof(Vertex3D),
		maxVertices,
		maxTriangles,
		coneWeight
	);

	m_meshlets.reserve(m_meshlets.size() + meshletCount);
	for (uint32_t meshletIndex = 0; meshletIndex < meshletCount; ++meshletIndex)
	{
	    Meshlet& meshlet = m_meshlets.emplace_back();
		outMeshletIndices.push_back(m_meshlets.size() - 1);
	    const meshopt_Meshlet& meshoptMeshlet = meshlets[meshletIndex];

	    meshlet.m_staticVertices.resize(meshoptMeshlet.vertex_count);

	    glm::vec3 minBounds(std::numeric_limits<float>::max());
	    glm::vec3 maxBounds(-std::numeric_limits<float>::max());

	    for (uint32_t i = 0; i < meshoptMeshlet.vertex_count; ++i)
	    {
	        uint32_t globalVertexIndex = meshletVertices[meshoptMeshlet.vertex_offset + i];
	        const Vertex3D& vertex = vertices[globalVertexIndex];

	        meshlet.m_staticVertices[i] = vertex;

	        glm::vec3 pos(vertex.pos[0], vertex.pos[1], vertex.pos[2]);
	        minBounds = glm::min(minBounds, pos);
	        maxBounds = glm::max(maxBounds, pos);
	    }
	    meshlet.m_aabb = Wolf::AABB(minBounds, maxBounds);

	    meshopt_Bounds bounds = meshopt_computeMeshletBounds(
	        &meshletVertices[meshoptMeshlet.vertex_offset],
	        &meshletTriangles[meshoptMeshlet.triangle_offset],
	        meshoptMeshlet.triangle_count,
	        &vertices[0].pos[0],
	        vertices.size(),
	        sizeof(Vertex3D)
	    );

	    meshlet.m_boundingSphere = Wolf::BoundingSphere(glm::vec3(bounds.center[0], bounds.center[1], bounds.center[2]), bounds.radius);
		if (bounds.radius <= 0.0f || std::isnan(bounds.radius))
		{
			Wolf::Debug::sendCriticalError("Bounding sphere radius is incorrect");
		}
		meshlet.m_groupBoundingSphere = groupBoundingSphere;

		meshlet.m_coneAxis[0] = bounds.cone_axis_s8[0];
		meshlet.m_coneAxis[1] = bounds.cone_axis_s8[1];
		meshlet.m_coneAxis[2] = bounds.cone_axis_s8[2];
		meshlet.m_coneCutoff = bounds.cone_cutoff_s8;

	    uint32_t indexCount = meshoptMeshlet.triangle_count * 3;
	    meshlet.m_indices.resize(indexCount);
	    const unsigned char* triangleIndices = &meshletTriangles[meshoptMeshlet.triangle_offset];
	    for (uint32_t i = 0; i < indexCount; ++i)
	    {
	        meshlet.m_indices[i] = static_cast<uint8_t>(triangleIndices[i]);
	    }

		meshlet.m_lodError = currentLodError;
		meshlet.m_parentLodError = 1e30f;
	}
}

void MeshFormatter::readMeshlets()
{
	for (uint32_t meshletIdx = 0; meshletIdx < m_meshlets.size(); ++meshletIdx)
	{
		Meshlet& meshlet = m_meshlets[meshletIdx];

		std::string meshletFilepath = m_cacheFolder + "meshlet" + std::to_string(meshletIdx) + ".bin";
		std::ifstream meshletCacheFile(meshletFilepath, std::ios::in | std::ios::binary);
		if (!meshletCacheFile.is_open())
		{
			Wolf::Debug::sendCriticalError("Can't open cache file for reading");
		}

		CacheHelper::readVector(meshletCacheFile, meshlet.m_staticVertices);
		CacheHelper::readVector(meshletCacheFile, meshlet.m_indices);

		meshletCacheFile.read(reinterpret_cast<char*>(&meshlet.m_aabb), sizeof(Wolf::AABB));
		meshletCacheFile.read(reinterpret_cast<char*>(&meshlet.m_boundingSphere), sizeof(Wolf::BoundingSphere));
		meshletCacheFile.read(reinterpret_cast<char*>(&meshlet.m_groupBoundingSphere), sizeof(Wolf::BoundingSphere));
		meshletCacheFile.read(reinterpret_cast<char*>(&meshlet.m_parentGroupBoundingSphere), sizeof(Wolf::BoundingSphere));
		meshletCacheFile.read(reinterpret_cast<char*>(&meshlet.m_coneAxis), sizeof(uint8_t) * 3);
		meshletCacheFile.read(reinterpret_cast<char*>(&meshlet.m_coneCutoff), sizeof(uint8_t));

		CacheHelper::readVector(meshletCacheFile, meshlet.m_parentMeshletIndices);

		meshletCacheFile.read(reinterpret_cast<char*>(&meshlet.m_lodError), sizeof(float));
		meshletCacheFile.read(reinterpret_cast<char*>(&meshlet.m_parentLodError), sizeof(float));

		meshletCacheFile.close();
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

std::vector<MeshFormatter::MeshletGroup> MeshFormatter::computeMeshletGroups(const std::vector<uint32_t>& meshlets, uint32_t targetGroupSize)
{
	uint32_t meshletCount = static_cast<uint32_t>(meshlets.size());
	if (meshletCount == 0)
	{
	    Wolf::Debug::sendCriticalError("There's no meshlet to group");
	}
	uint32_t firstMeshlet = meshlets.front();

	auto positionHash = [](const Vertex3D& v) -> uint64_t
	{
	    int32_t x = static_cast<int32_t>(std::round(v.pos[0] * 1000.0f));
	    int32_t y = static_cast<int32_t>(std::round(v.pos[1] * 1000.0f));
	    int32_t z = static_cast<int32_t>(std::round(v.pos[2] * 1000.0f));

	    uint64_t h = 14695981039346656037ULL;
	    h = (h ^ x) * 1099511628211ULL;
	    h = (h ^ y) * 1099511628211ULL;
	    h = (h ^ z) * 1099511628211ULL;
	    return h;
	};

	std::unordered_map<uint64_t, std::vector<uint32_t>> vertexToMeshlets;
	for (uint32_t i = 0; i < meshletCount; ++i)
	{
	    for (const Vertex3D& vertex : m_meshlets[meshlets[i]].m_staticVertices)
	    {
	        uint64_t hashKey = positionHash(vertex);

	        std::vector<uint32_t>& list = vertexToMeshlets[hashKey];
	        if (list.empty() || list.back() != i)
	        {
	            list.push_back(i);
	        }
	    }
	}

	std::vector<std::unordered_map<uint32_t /* meshlet idx */, uint32_t /* shared vertex count */>> adjacency(meshletCount);
	for (const std::vector<uint32_t>& containingMeshlets : vertexToMeshlets | std::views::values)
	{
	    for (size_t i = 0; i < containingMeshlets.size(); ++i)
	    {
	        for (size_t j = i + 1; j < containingMeshlets.size(); ++j)
	        {
	            uint32_t mA = containingMeshlets[i];
	            uint32_t mB = containingMeshlets[j];

	            adjacency[mA][mB]++;
	            adjacency[mB][mA]++;
	        }
	    }
	}

	std::vector<bool> addedToAGroup(meshletCount, false);
	std::vector<MeshletGroup> groups;

	for (uint32_t i = 0; i < meshletCount; ++i)
	{
	    if (addedToAGroup[i])
	    {
	        continue;
	    }

	    MeshletGroup group;
	    group.m_meshletIndices.push_back(i);
	    addedToAGroup[i] = true;

	    while (group.m_meshletIndices.size() < targetGroupSize)
	    {
	        uint32_t bestCandidateIdx = -1;
	        uint32_t maxSharedVertices = 0;

	        for (uint32_t memberIdx : group.m_meshletIndices)
	        {
	            for (const auto& [neighborIdx, sharedCount] : adjacency[memberIdx])
	            {
	                if (!addedToAGroup[neighborIdx] && sharedCount > maxSharedVertices)
	                {
	                    maxSharedVertices = sharedCount;
	                    bestCandidateIdx = neighborIdx;
	                }
	            }
	        }

	        // Fall back to spatial proximity if topology is disconnected
	        if (bestCandidateIdx == -1)
	        {
	            float minDistanceSq = std::numeric_limits<float>::max();

	            glm::vec3 groupCentroid(0.0f);
	            for (uint32_t memberIdx : group.m_meshletIndices)
	            {
            		groupCentroid += m_meshlets[meshlets[memberIdx]].m_boundingSphere.getCenter();
	            }
	            groupCentroid /= static_cast<float>(group.m_meshletIndices.size());

	            for (uint32_t candidateIdx = 0; candidateIdx < meshletCount; ++candidateIdx)
	            {
            		if (!addedToAGroup[candidateIdx])
            		{
            			const glm::vec3& centerB = m_meshlets[meshlets[candidateIdx]].m_boundingSphere.getCenter();
            			float distSq = glm::distance2(groupCentroid, centerB);

            			if (distSq < minDistanceSq)
            			{
            				minDistanceSq = distSq;
            				bestCandidateIdx = static_cast<int32_t>(candidateIdx);
            			}
            		}
	            }
	        }

	        if (bestCandidateIdx != -1)
	        {
	            group.m_meshletIndices.push_back(bestCandidateIdx);
	            addedToAGroup[bestCandidateIdx] = true;
	        }
	        else
	        {
	            break;
	        }
	    }

		for (uint32_t& meshletIdx : group.m_meshletIndices)
		{
			meshletIdx += firstMeshlet;
		}
	    groups.push_back(group);
	}

	return groups;
}

void MeshFormatter::computeMeshletsForGroup(const MeshletGroup& group, std::vector<uint32_t>& outMeshletIndices, bool isRootGroup)
{
	if (group.m_meshletIndices.empty())
    {
        Wolf::Debug::sendCriticalError("Can't compute meshlets for a empty group");
    }

	float maxChildError = 0.0f;
	for (uint32_t childIdx : group.m_meshletIndices)
	{
		maxChildError = std::max(maxChildError, m_meshlets[childIdx].m_lodError);
	}

    std::vector<Vertex3D> mergedVertices;
    std::vector<uint32_t> mergedIndices;

	std::unordered_map<Vertex3D, uint32_t> vertexRemap;
	for (uint32_t meshletIdx : group.m_meshletIndices)
	{
		const Meshlet& childMeshlet = m_meshlets[meshletIdx];

		for (uint8_t index : childMeshlet.m_indices)
		{
			const Vertex3D& v = childMeshlet.m_staticVertices[index];

			// Deduplicate using Vertex3D hash, same position vertices will be merge by meshoptimizer simplification
			if (auto it = vertexRemap.find(v); it != vertexRemap.end())
			{
				mergedIndices.push_back(it->second);
			}
			else
			{
				uint32_t newIdx = static_cast<uint32_t>(mergedVertices.size());
				mergedVertices.push_back(v);
				vertexRemap[v] = newIdx;
				mergedIndices.push_back(newIdx);
			}
		}
	}

    if (mergedIndices.empty())
    {
    	Wolf::Debug::sendCriticalError("No index");
    }

	glm::vec3 minPos(1'000'000.f);
	glm::vec3 maxPos(-1'000'000.f);

	for (uint32_t idx : mergedIndices)
	{
		const Vertex3D& vertex = mergedVertices[idx];

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

	Wolf::AABB groupAABB = Wolf::AABB(minPos, maxPos);
	Wolf::BoundingSphere groupSphere(groupAABB);

	size_t targetTriangles = std::max<size_t>(1, mergedIndices.size() / (3 * 3));
	size_t targetIndexCount = targetTriangles * 3;

    std::vector<uint32_t> simplifiedIndices(mergedIndices.size());
    float targetError = 1.0f;
	unsigned int options = isRootGroup ? 0 : meshopt_SimplifyLockBorder;

	float simplificationError = 0.0f;

	constexpr float attributeWeights[] = { 1.0f, 1.0f };
	const float* vertexAttributes = reinterpret_cast<const float*>(
		reinterpret_cast<const char*>(mergedVertices.data()) + offsetof(Vertex3D, texCoord)
	);

    size_t newIndexCount = meshopt_simplifyWithAttributes(
        simplifiedIndices.data(),
        mergedIndices.data(),
        mergedIndices.size(),
        reinterpret_cast<const float*>(mergedVertices.data()),
        mergedVertices.size(),
        sizeof(Vertex3D),
        vertexAttributes,
        sizeof(Vertex3D),
        attributeWeights,
        2,
        targetIndexCount,
        targetError,
        options,
        &simplificationError);

    simplifiedIndices.resize(newIndexCount);

	float parentNodeLodError = maxChildError + simplificationError;

	for (uint32_t childIdx : group.m_meshletIndices)
	{
		m_meshlets[childIdx].m_parentLodError = std::min(m_meshlets[childIdx].m_parentLodError, parentNodeLodError);
		m_meshlets[childIdx].m_parentGroupBoundingSphere = groupSphere;
	}

	computeMeshlets(mergedVertices, simplifiedIndices, outMeshletIndices, parentNodeLodError, groupSphere);
}
