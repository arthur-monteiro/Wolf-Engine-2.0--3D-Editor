#include "MaterialListFakeEntity.h"

#include "EditorParamsHelper.h"

MaterialListFakeEntity::MaterialListFakeEntity(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration)
	: Entity("", [](Entity*){}), m_materialsGPUManager(materialsGPUManager), m_editorConfiguration(editorConfiguration)
{
}

void MaterialListFakeEntity::activateParams() const
{
	for (EditorParamInterface* param : m_editorParams)
	{
		param->activate();
	}
}

void MaterialListFakeEntity::fillJSONForParams(std::string& outJSON)
{
	m_materialCount = m_materialsGPUManager->getCurrentMaterialCount();
	m_materials.clear();
	std::vector<Wolf::MaterialsGPUManager::MaterialCacheInfo>& materialCache = m_materialsGPUManager->getMaterialsCacheInfo();
	for (uint32_t i = 0; i < materialCache.size(); ++i)
	{
		Wolf::MaterialsGPUManager::MaterialCacheInfo& materialCacheInfo = materialCache[i];

		if (materialCacheInfo.materialInfo->imageNames.size() != 6)
		{
			Wolf::Debug::sendWarning("Debug info for material " + materialCacheInfo.materialInfo->materialName + " has been found but there isn't 6 image names");
			continue;
		}

		MaterialInfo& materialInfo = m_materials.emplace_back();
		materialInfo.setMaterialCacheInfo(materialCacheInfo);
		materialInfo.setName(materialCacheInfo.materialInfo->materialName);
		materialInfo.getMaterialEditor().setMaterialId(i + 1 /* remove default material */);

		auto computeLocalPath = [materialCacheInfo](uint32_t imageIdx) -> std::string
			{
				if (materialCacheInfo.materialInfo->imageNames[imageIdx].empty())
					return "";
				return g_editorConfiguration->computeLocalPathFromFullPath(materialCacheInfo.materialInfo->materialFolder + "/" + materialCacheInfo.materialInfo->imageNames[imageIdx]);
			};

		materialInfo.getMaterialEditor().setAlbedoPath(computeLocalPath(0));
		materialInfo.getMaterialEditor().setNormalPath(computeLocalPath(1));
		materialInfo.getMaterialEditor().setRoughnessPath(computeLocalPath(2));
		materialInfo.getMaterialEditor().setMetalnessPath(computeLocalPath(3));
		materialInfo.getMaterialEditor().setAOPath(computeLocalPath(4));
		materialInfo.getMaterialEditor().setAnisoStrengthPath(computeLocalPath(5));
		materialInfo.getMaterialEditor().setShadingMode(materialCacheInfo.materialInfo->shadingMode);
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

void MaterialListFakeEntity::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
	m_materials.lockAccessElements();

	for (uint32_t i = 0; i < m_materials.size(); ++i)
	{
		MaterialInfo& materialEditor = m_materials[i];
		materialEditor.updateBeforeFrame(m_materialsGPUManager, m_editorConfiguration);
	}

	m_materials.unlockAccessElements();
}

void MaterialListFakeEntity::save()
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

std::span<EditorParamInterface*> MaterialListFakeEntity::MaterialInfo::getAllParams()
{
	return m_material->getAllParams();
}

std::span<EditorParamInterface* const> MaterialListFakeEntity::MaterialInfo::getAllConstParams() const
{
	return m_material->getAllConstParams();
}

bool MaterialListFakeEntity::MaterialInfo::hasDefaultName() const
{
	return false;
}

void MaterialListFakeEntity::MaterialInfo::setMaterialCacheInfo(Wolf::MaterialsGPUManager::MaterialCacheInfo& materialCacheInfo)
{
	m_material.reset(new MaterialEditor("Material List", "Material", materialCacheInfo));
}
