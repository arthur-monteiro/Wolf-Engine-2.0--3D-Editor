#include "ExternalSceneLoader.h"

#include "GLTFImporter.h"

void ExternalSceneLoader::loadScene(OutputData& outputData, SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager)
{
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
