#pragma once

#include "EditorTypesTemplated.h"
#include "Entity.h"
#include "TextureSetEditor.h"
#include "Notifier.h"
#include "ParameterGroupInterface.h"

class MaterialListFakeEntity : public Entity
{
public:
	MaterialListFakeEntity(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

	void activateParams() override;
	void fillJSONForParams(std::string& outJSON) override;

	void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager, const Wolf::ResourceNonOwner<EditorPhysicsManager>& editorPhysicsManager) override;

	void save() const override;

	const std::string& getName() const override { return m_name; }
	std::string computeEscapedLoadingPath() const override { return "materialsListId"; }
	bool isFake() const override { return true; }

private:
	std::string m_name = "Material List";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;

	class MaterialInfo : public ParameterGroupInterface, public Notifier
	{
	public:
		MaterialInfo();
		MaterialInfo(const MaterialInfo&) = delete;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override;

		TextureSetEditor& getMaterialEditor() { return *m_material; }

		void setMaterialCacheInfo();

	private:
		Wolf::ResourceUniqueOwner<TextureSetEditor> m_material;
	};

	EditorParamUInt m_materialCount = EditorParamUInt("Material Count", "Material List", "General", 0, 100, EditorParamUInt::ParamUIntType::NUMBER, false, true);
	static constexpr uint32_t MAX_MATERIALS = 1024;
	EditorParamArray<MaterialInfo> m_materials = EditorParamArray<MaterialInfo>("Materials", "Material List", "Materials", MAX_MATERIALS, false, true);

	std::array<EditorParamInterface*, 2> m_editorParams =
	{
		&m_materialCount,
		&m_materials
	};
};

