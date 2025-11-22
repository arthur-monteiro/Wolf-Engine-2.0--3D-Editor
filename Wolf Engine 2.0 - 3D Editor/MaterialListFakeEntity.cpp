#include "MaterialListFakeEntity.h"

#include "EditorParamsHelper.h"

MaterialListFakeEntity::MaterialListFakeEntity(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
	: Entity("", [](Entity*){}, [](Entity*) {}), m_materialsGPUManager(materialsGPUManager), m_editorConfiguration(editorConfiguration)
{
}

void MaterialListFakeEntity::activateParams()
{
	for (EditorParamInterface* param : m_editorParams)
	{
		param->activate();
	}
}

void MaterialListFakeEntity::fillJSONForParams(std::string& outJSON)
{
	m_materialCount = m_materialsGPUManager->getCurrentTextureSetCount();
	m_materials.clear();
	std::vector<Wolf::MaterialsGPUManager::TextureSetCacheInfo>& textureSetCache = m_materialsGPUManager->getTextureSetsCacheInfo();
	for (uint32_t i = 0; i < textureSetCache.size(); ++i)
	{
		Wolf::MaterialsGPUManager::TextureSetCacheInfo& materialCacheInfo = textureSetCache[i];

		if (materialCacheInfo.materialName.empty())
			continue;

		if (materialCacheInfo.imageNames.size() != 6)
		{
			Wolf::Debug::sendWarning("Debug info for material " + materialCacheInfo.materialName + " has been found but there isn't 6 image names");
			continue;
		}

		MaterialInfo& materialInfo = m_materials.emplace_back();
		materialInfo.setMaterialCacheInfo();
		materialInfo.setName(materialCacheInfo.materialName);

		auto computeLocalPath = [materialCacheInfo](uint32_t imageIdx) -> std::string
			{
				if (materialCacheInfo.imageNames[imageIdx].empty())
					return "";
				return g_editorConfiguration->computeLocalPathFromFullPath(materialCacheInfo.materialFolder + "/" + materialCacheInfo.imageNames[imageIdx]);
			};

		materialInfo.getMaterialEditor().setAlbedoPath(computeLocalPath(0));
		materialInfo.getMaterialEditor().setNormalPath(computeLocalPath(1));
		materialInfo.getMaterialEditor().setRoughnessPath(computeLocalPath(2));
		materialInfo.getMaterialEditor().setMetalnessPath(computeLocalPath(3));
		materialInfo.getMaterialEditor().setAOPath(computeLocalPath(4));
		materialInfo.getMaterialEditor().setAnisoStrengthPath(computeLocalPath(5));
	}

	activateParams(); // reactivate params as we just modified them

	outJSON += "{\n";
	outJSON += "\t" R"("params": [)" "\n";
	addParamsToJSON(outJSON, m_editorParams, true);
	if (const size_t commaPos = outJSON.substr(outJSON.size() - 3).find(','); commaPos != std::string::npos)
	{
		outJSON.erase(commaPos + outJSON.size() - 3);
	}
	outJSON += "\t]\n";
	outJSON += "}";
}

void MaterialListFakeEntity::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager, const Wolf::ResourceNonOwner<EditorPhysicsManager>& editorPhysicsManager)
{
	m_materials.lockAccessElements();

	// Little hack to avoid the texture set editors to create their own resources
	/*for (uint32_t i = 0; i < m_materials.size(); ++i)
	{
		MaterialInfo& materialEditor = m_materials[i];
		materialEditor.updateBeforeFrame(m_materialsGPUManager, m_editorConfiguration);
	}*/

	m_materials.unlockAccessElements();
}

void MaterialListFakeEntity::save() const
{
	// TODO: save info in .mtl file
}

MaterialListFakeEntity::MaterialInfo::MaterialInfo() : ParameterGroupInterface("Material List")
{
}

void MaterialListFakeEntity::MaterialInfo::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
{
	m_material->updateBeforeFrame(materialGPUManager, editorConfiguration);
}

void MaterialListFakeEntity::MaterialInfo::getAllParams(std::vector<EditorParamInterface*>& out) const
{
	m_material->getAllParams(out);
}

void MaterialListFakeEntity::MaterialInfo::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
	m_material->getAllVisibleParams(out);
}

bool MaterialListFakeEntity::MaterialInfo::hasDefaultName() const
{
	return false;
}

void MaterialListFakeEntity::MaterialInfo::setMaterialCacheInfo()
{
	m_material.reset(new TextureSetEditor("Material List", "Material", Wolf::MaterialsGPUManager::MaterialInfo::ShadingMode::GGX));
}
