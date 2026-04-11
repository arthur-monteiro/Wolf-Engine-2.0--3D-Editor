#include "ExternalSceneLoader.h"

#include <filesystem>

#include "AssetManager.h"
#include "CacheHelper.h"
#include "EditorConfiguration.h"
#include "GLTFImporter.h"

void ExternalSceneLoader::loadScene(OutputData& outputData, SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager)
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
    else
    {
        Wolf::Debug::sendCriticalError("Unhandled filename extension");
    }
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
        CacheHelper::writeVector(file, mesh.m_indices);
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
        CacheHelper::readVector(file, mesh.m_indices);
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

        TextureSetLoader::OutputLayout outputLayout;
        outputLayout.albedoCompression = Wolf::ImageCompression::Compression::BC1;
        outputLayout.normalCompression = Wolf::ImageCompression::Compression::BC5;

        TextureSetLoader textureSetLoader(outputMaterialData.m_textureSetFileInfo, outputLayout, true, assetManager);

        for (uint32_t i = 0; i < 2 /* albedo and normal */; ++i)
        {
            if (AssetId assetId = textureSetLoader.getImageAssetId(i); assetId != NO_ASSET)
            {
                outputMaterialData.m_textureSet.imageAssetIds[i] = assetId;

                outputMaterialData.m_textureSet.images[i] = assetManager->getImage(textureSetLoader.getImageAssetId(i));
                outputMaterialData.m_textureSet.slicesFolders[i] = assetManager->getImageSlicesFolder(textureSetLoader.getImageAssetId(i));
            }
        }

        // Combined image
        if (AssetId assetId = textureSetLoader.getImageAssetId(2); assetId != NO_ASSET)
        {
            outputMaterialData.m_textureSet.imageAssetIds[2] = assetId;

            outputMaterialData.m_textureSet.images[2] = assetManager->getCombinedImage(textureSetLoader.getImageAssetId(2));
            outputMaterialData.m_textureSet.slicesFolders[2] = assetManager->getCombinedImageSlicesFolder(textureSetLoader.getImageAssetId(2));
        }
    }

    CacheHelper::readVector(file, outData.m_instancesData);

    file.close();
}
