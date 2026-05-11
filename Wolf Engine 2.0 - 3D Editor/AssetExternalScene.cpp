#include "AssetExternalScene.h"

#include "AssetManager.h"

AssetExternalScene::AssetExternalScene(AssetManager* assetManager, const std::string& loadingPath, bool needThumbnailsGeneration, AssetId assetId,
	const std::function<void(const std::string&, const std::string&, AssetId)>& updateResourceInUICallback, AssetId parentAssetId)
: AssetInterface(loadingPath, assetId, updateResourceInUICallback, parentAssetId), m_assetManager(assetManager)
{
	m_loadingRequested = false;

	m_editor.reset(new ExternalSceneAssetEditor());
	m_editor->subscribe(this, [this](Flags)
	{
		if (!m_editor->getLoadingPath().empty())
		{
			m_loadingRequested = true;
		}
	});

	const std::ifstream inFile(g_editorConfiguration->computeFullPathFromLocalPath(loadingPath));
	if (!loadingPath.empty() && inFile.good())
	{
		Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { g_editorConfiguration->computeFullPathFromLocalPath(loadingPath) });
		m_editor->loadParams(jsonReader);
	}
}

void AssetExternalScene::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	if (m_loadingRequested)
	{
		loadScene(materialsGPUManager);
		m_loadingRequested = false;
	}
	if (m_thumbnailGenerationRequested)
	{
		generateThumbnail(thumbnailsGenerationPass);
		m_thumbnailGenerationRequested = false;
	}
}

bool AssetExternalScene::isLoaded() const
{
	return !m_meshAssetIds.empty();
}

const Wolf::AABB& AssetExternalScene::getAABB()
{
	return m_aabb;
}

void AssetExternalScene::loadScene(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager)
{
	ExternalSceneLoader::SceneLoadingInfo sceneLoadingInfo;
	sceneLoadingInfo.filename = m_editor->getLoadingPath();

	ExternalSceneLoader::OutputData outputData{};
	ExternalSceneLoader::loadScene(outputData, sceneLoadingInfo, m_assetManager);

	/* Add materials */
	std::vector<uint32_t> materialGPUIndices;
	materialGPUIndices.reserve(outputData.m_materialsData.size());
	for (const ExternalSceneLoader::MaterialData& materialData : outputData.m_materialsData)
	{
		const TextureSetLoader::TextureSetFileInfoGGX& textureSetFileInfo = materialData.m_textureSetFileInfo;
		AssetId textureSetAssetId = m_assetManager->addTextureSet(sceneLoadingInfo.filename + "_" + materialData.m_textureSetFileInfo.name + "_textureSet", m_assetId);
		Wolf::ResourceNonOwner<TextureSetEditor> textureSetEditor = m_assetManager->getTextureSetEditor(textureSetAssetId);

		if (!textureSetFileInfo.albedo.empty())
		{
			AssetId albedoAssetId = m_assetManager->addImage(textureSetFileInfo.albedo + "_image", m_assetId);
			m_assetManager->getImageEditor(albedoAssetId)->setLoadingPath(textureSetFileInfo.albedo);
			textureSetEditor->setAlbedoPath(textureSetFileInfo.albedo + "_image");
		}

		if (!textureSetFileInfo.normal.empty())
		{
			AssetId normalAssetId = m_assetManager->addImage(textureSetFileInfo.normal + "_image", m_assetId);
			m_assetManager->getImageEditor(normalAssetId)->setLoadingPath(textureSetFileInfo.normal);
			textureSetEditor->setNormalPath(textureSetFileInfo.normal + "_image");
		}

		if (!textureSetFileInfo.roughness.empty())
		{
			AssetId roughnessAssetId = m_assetManager->addImage(textureSetFileInfo.roughness + "_image", m_assetId);
			m_assetManager->getImageEditor(roughnessAssetId)->setLoadingPath(textureSetFileInfo.roughness);
			textureSetEditor->setRoughnessPath(textureSetFileInfo.roughness + "_image");
		}

		if (!textureSetFileInfo.metalness.empty())
		{
			AssetId metalnessAssetId = m_assetManager->addImage(textureSetFileInfo.metalness + "_image", m_assetId);
			m_assetManager->getImageEditor(metalnessAssetId)->setLoadingPath(textureSetFileInfo.metalness);
			textureSetEditor->setMetalnessPath(textureSetFileInfo.metalness + "_image");
		}

		if (!textureSetFileInfo.ao.empty())
		{
			AssetId aoAssetId = m_assetManager->addImage(textureSetFileInfo.ao + "_image");
			m_assetManager->getImageEditor(aoAssetId)->setLoadingPath(textureSetFileInfo.ao);
			textureSetEditor->setAOPath(textureSetFileInfo.ao + "_image");
		}

		textureSetEditor->updateBeforeFrame();

		AssetId materialAssetId = m_assetManager->addMaterial(sceneLoadingInfo.filename + "_" + materialData.m_textureSetFileInfo.name + "_material", m_assetId);
		Wolf::ResourceNonOwner<MaterialEditor> materialEditor = m_assetManager->getMaterialEditor(materialAssetId);

		materialEditor->addTextureSet(sceneLoadingInfo.filename + "_" + materialData.m_textureSetFileInfo.name + "_textureSet", 1.0f);
		materialEditor->updateBeforeFrame();

		materialGPUIndices.push_back(materialEditor->getMaterialGPUIdx());
	}

	/* Add meshes */
	m_meshAssetIds.reserve(outputData.m_meshesData.size());
	for (uint32_t meshIdx = 0; meshIdx < outputData.m_meshesData.size(); meshIdx++)
	{
		ExternalSceneLoader::MeshData& meshData = outputData.m_meshesData[meshIdx];
		uint32_t materialIdx = -1;
		for (uint32_t instanceIdx = 0; instanceIdx < outputData.m_instancesData.size(); ++instanceIdx)
		{
			ExternalSceneLoader::InstanceData& instanceData = outputData.m_instancesData[instanceIdx];
			if (instanceData.m_meshIdx == meshIdx)
			{
				materialIdx = instanceData.m_materialIdx == -1 ? 0 : materialGPUIndices[instanceData.m_materialIdx];
				break;
			}
		}

		if (materialIdx == -1)
		{
			Wolf::Debug::sendCriticalError("No instance has been found for this mesh");
		}

		m_meshAssetIds.push_back(m_assetManager->addMesh(meshData, sceneLoadingInfo.filename + "_mesh_" + meshData.m_name + "_" + std::to_string(meshIdx), materialIdx, m_assetId));
	}

	//m_aabb = Wolf::AABB(sceneAABBMin, sceneAABBMax);
	m_instances = outputData.m_instancesData;
}

void AssetExternalScene::generateThumbnail(const Wolf::ResourceNonOwner<ThumbnailsGenerationPass>& thumbnailsGenerationPass)
{
	// TODO
}
