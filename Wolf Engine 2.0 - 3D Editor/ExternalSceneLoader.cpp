#include "ExternalSceneLoader.h"

#include <filesystem>

#include "AssetManager.h"
#include "CacheHelper.h"
#include "EditorConfiguration.h"
#include "GLTFImporter.h"
#include "OBJImporter.h"

void ExternalSceneLoader::loadScene(OutputData& outputData, const SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager)
{
    std::string binFilename = g_editorConfiguration->computeFullPathFromLocalPath(sceneLoadingInfo.filename + ".bin");
    if (std::filesystem::exists(binFilename))
    {
        loadCache(binFilename, outputData, assetManager);
        return;
    }

    std::string filenameExtension = sceneLoadingInfo.filename.substr(sceneLoadingInfo.filename.find_last_of(".") + 1);
    if (filenameExtension == "gltf")
    {
        GLTFImporter gltfLoader(outputData, sceneLoadingInfo, assetManager);
    }
    else if (filenameExtension == "obj")
    {
        OBJImporter objImporter(outputData, sceneLoadingInfo, assetManager);
    }
    else if (filenameExtension == "dae")
    {
        DAEImporter daeImporter(outputData, sceneLoadingInfo, assetManager);
    }
    else
    {
        Wolf::Debug::sendCriticalError("Unhandled filename extension");
    }

    // TODO: fix physics loading
    // Load physics
	// std::string physicsConfigFilename = modelLoadingInfo.filename + ".physics.json";
	// if (std::filesystem::exists(physicsConfigFilename))
	// {
	// 	Wolf::JSONReader physicsJSONReader(Wolf::JSONReader::FileReadInfo{ physicsConfigFilename });
	//
	// 	uint32_t physicsMeshesCount = static_cast<uint32_t>(physicsJSONReader.getRoot()->getPropertyFloat("physicsMeshesCount"));
	// 	outputModel.m_physicsShapes.resize(physicsMeshesCount);
	//
	// 	for (uint32_t i = 0; i < physicsMeshesCount; ++i)
	// 	{
	// 		Wolf::JSONReader::JSONObjectInterface* physicsMeshObject = physicsJSONReader.getRoot()->getArrayObjectItem("physicsMeshes", i);
	// 		std::string type = physicsMeshObject->getPropertyString("type");
	//
	// 		if (type == "Rectangle")
	// 		{
	// 			glm::vec3 p0 = glm::vec3(physicsMeshObject->getPropertyFloat("defaultP0X"), physicsMeshObject->getPropertyFloat("defaultP0Y"), physicsMeshObject->getPropertyFloat("defaultP0Z"));
	// 			glm::vec3 s1 = glm::vec3(physicsMeshObject->getPropertyFloat("defaultS1X"), physicsMeshObject->getPropertyFloat("defaultS1Y"), physicsMeshObject->getPropertyFloat("defaultS1Z"));
	// 			glm::vec3 s2 = glm::vec3(physicsMeshObject->getPropertyFloat("defaultS2X"), physicsMeshObject->getPropertyFloat("defaultS2Y"), physicsMeshObject->getPropertyFloat("defaultS2Z"));
	//
	// 			outputModel.m_physicsShapes[i].reset(new Wolf::Physics::Rectangle(physicsMeshObject->getPropertyString("name"), p0, s1, s2));
	// 		}
	// 		else if (type == "Box")
	// 		{
	// 			glm::vec3 aabbMin = glm::vec3(physicsMeshObject->getPropertyFloat("defaultAABBMinX"), physicsMeshObject->getPropertyFloat("defaultAABBMinY"), physicsMeshObject->getPropertyFloat("defaultAABBMinZ"));
	// 			glm::vec3 aabbMax = glm::vec3(physicsMeshObject->getPropertyFloat("defaultAABBMaxX"), physicsMeshObject->getPropertyFloat("defaultAABBMaxY"), physicsMeshObject->getPropertyFloat("defaultAABBMaxZ"));
	// 			glm::vec3 rotation = glm::vec3(physicsMeshObject->getPropertyFloat("defaultRotationX"), physicsMeshObject->getPropertyFloat("defaultRotationY"), physicsMeshObject->getPropertyFloat("defaultRotationZ"));
	//
	// 			outputModel.m_physicsShapes[i].reset(new Wolf::Physics::Box(physicsMeshObject->getPropertyString("name"), aabbMin, aabbMax, rotation));
	// 		}
	// 		else
	// 		{
	// 			Wolf::Debug::sendCriticalError("Unhandled physics mesh type");
	// 		}
	// 	}
	// }

    std::string cacheFilename = g_editorConfiguration->computeFullPathFromLocalPath(sceneLoadingInfo.filename + ".bin");
    writeCache(cacheFilename, outputData);
}

void ExternalSceneLoader::writeCache(const std::string& filename, const OutputData& data)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        Wolf::Debug::sendCriticalError("Cannot open file for writing");
        return;
    }

    uint32_t meshCount = static_cast<uint32_t>(data.m_meshesData.size());
    file.write(reinterpret_cast<const char*>(&meshCount), sizeof(uint32_t));

    for (const MeshData& mesh : data.m_meshesData)
    {
        CacheHelper::writeString(file, mesh.m_name);
        CacheHelper::writeVector(file, mesh.m_staticVertices);
        CacheHelper::writeVector(file, mesh.m_skeletonVertices);
        CacheHelper::writeVector(file, mesh.m_indices);

        uint8_t hasAnimationData = static_cast<bool>(mesh.m_animationData);
        file.write(reinterpret_cast<const char*>(&hasAnimationData), sizeof(hasAnimationData));

        if (hasAnimationData)
        {
            file.write(reinterpret_cast<const char*>(&mesh.m_animationData->m_boneCount), sizeof(mesh.m_animationData->m_boneCount));

            uint32_t rootCount = static_cast<uint32_t>(mesh.m_animationData->m_rootBones.size());
            file.write(reinterpret_cast<const char*>(&rootCount), sizeof(uint32_t));
            for (const AnimationData::Bone& root : mesh.m_animationData->m_rootBones)
            {
                writeBoneToCache(file, root);
            }
        }
    }

    uint32_t matCount = static_cast<uint32_t>(data.m_materialsData.size());
    file.write(reinterpret_cast<const char*>(&matCount), sizeof(uint32_t));

    for (const MaterialData& mat : data.m_materialsData)
    {
        const auto& [name, albedo, normal, roughness, metalness, ao, anisoStrength] = mat.m_textureSetFileInfo;
        CacheHelper::writeString(file, name);
        CacheHelper::writeString(file, albedo);
        CacheHelper::writeString(file, normal);
        CacheHelper::writeString(file, roughness);
        CacheHelper::writeString(file, metalness);
        CacheHelper::writeString(file, ao);
        CacheHelper::writeString(file, anisoStrength);
    }

    CacheHelper::writeVector(file, data.m_instancesData);

    file.close();
}

void ExternalSceneLoader::loadCache(const std::string& filename, OutputData& outData, AssetManager* assetManager)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        Wolf::Debug::sendCriticalError("Cannot open file for reading");
        return;
    }

    uint32_t meshCount = 0;
    file.read(reinterpret_cast<char*>(&meshCount), sizeof(uint32_t));
    outData.m_meshesData.resize(meshCount);

    for (MeshData& mesh : outData.m_meshesData)
    {
        CacheHelper::readString(file, mesh.m_name);
        CacheHelper::readVector(file, mesh.m_staticVertices);
        CacheHelper::readVector(file, mesh.m_skeletonVertices);
        CacheHelper::readVector(file, mesh.m_indices);

        uint8_t hasAnimationData = 0;
        file.read(reinterpret_cast<char*>(&hasAnimationData), sizeof(hasAnimationData));

        if (hasAnimationData)
        {
            mesh.m_animationData.reset(new AnimationData());

            file.read(reinterpret_cast<char*>(&mesh.m_animationData->m_boneCount), sizeof(uint32_t));

            uint32_t rootCount = 0;
            file.read(reinterpret_cast<char*>(&rootCount), sizeof(uint32_t));
            mesh.m_animationData->m_rootBones.resize(rootCount);
            for (uint32_t i = 0; i < rootCount; ++i)
            {
                readBoneFromCache(file, mesh.m_animationData->m_rootBones[i]);
            }
        }
    }

    uint32_t matCount = 0;
    file.read(reinterpret_cast<char*>(&matCount), sizeof(uint32_t));
    outData.m_materialsData.resize(matCount);

    for (MaterialData& outputMaterialData : outData.m_materialsData)
    {
        auto& [name, albedo, normal, roughness, metalness, ao, anisoStrength] = outputMaterialData.m_textureSetFileInfo;
        CacheHelper::readString(file, name);
        CacheHelper::readString(file, albedo);
        CacheHelper::readString(file, normal);
        CacheHelper::readString(file, roughness);
        CacheHelper::readString(file, metalness);
        CacheHelper::readString(file, ao);
        CacheHelper::readString(file, anisoStrength);
    }

    CacheHelper::readVector(file, outData.m_instancesData);

    file.close();
}

void ExternalSceneLoader::writeBoneToCache(std::ofstream& file, const AnimationData::Bone& bone)
{
    file.write(reinterpret_cast<const char*>(&bone.m_idx), sizeof(uint32_t));
    CacheHelper::writeString(file, bone.m_name);
    file.write(reinterpret_cast<const char*>(&bone.m_offsetMatrix), sizeof(glm::mat4));
    CacheHelper::writeVector(file, bone.m_poses);

    uint32_t childCount = static_cast<uint32_t>(bone.m_children.size());
    file.write(reinterpret_cast<const char*>(&childCount), sizeof(uint32_t));
    for (const auto& child : bone.m_children)
    {
        writeBoneToCache(file, child);
    }
}

void ExternalSceneLoader::readBoneFromCache(std::ifstream& file, AnimationData::Bone& bone)
{
    file.read(reinterpret_cast<char*>(&bone.m_idx), sizeof(uint32_t));
    CacheHelper::readString(file, bone.m_name);
    file.read(reinterpret_cast<char*>(&bone.m_offsetMatrix), sizeof(glm::mat4));
    CacheHelper::readVector(file, bone.m_poses);

    uint32_t childCount = 0;
    file.read(reinterpret_cast<char*>(&childCount), sizeof(uint32_t));

    bone.m_children.resize(childCount);
    for (uint32_t i = 0; i < childCount; ++i)
    {
        readBoneFromCache(file, bone.m_children[i]);
    }
}
