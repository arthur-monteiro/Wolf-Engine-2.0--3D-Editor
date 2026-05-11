#pragma once

#include "ExternalSceneLoader.h"

class OBJImporter
{
public:
    OBJImporter(ExternalSceneLoader::OutputData& outputData, const ExternalSceneLoader::SceneLoadingInfo& sceneLoadingInfo, AssetManager* assetManager);
};