#pragma once

#include "ComponentInterface.h"
#include "EditorTypesTemplated.h"
#include "EditorTypes.h"
#include "TextureSetEditor.h"

class Particle : public ComponentInterface, public Notifier
{
public:
	static inline std::string ID = "particleComponent";
	std::string getId() const override { return ID; }

	Particle(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration, 
		const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	bool requiresInputs() const override { return false; }
	void saveCustom() const override {}

	uint32_t getMaterialIdx() const;
	uint32_t getFlipBookSizeX() const { return m_flipBookSizeX; }
	uint32_t getFlipBookSizeY() const { return m_flipBookSizeY; }

private:
	inline static const std::string TAB = "Particle";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialGPUManager;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_materialEntity;
	void onMaterialEntityChanged();
	bool m_materialNotificationRegistered = false;

	EditorParamString m_materialEntityParam = EditorParamString("Material entity", TAB, "Material", [this]() { onMaterialEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);
	EditorParamUInt m_flipBookSizeX = EditorParamUInt("Flip book size X", TAB, "Material", 1, 8, [this]() { notifySubscribers(); });
	EditorParamUInt m_flipBookSizeY = EditorParamUInt("Flip book size Y", TAB, "Material", 1, 8, [this]() { notifySubscribers(); });

	std::array<EditorParamInterface*, 3> m_editorParams =
	{
		&m_materialEntityParam,
		&m_flipBookSizeX,
		&m_flipBookSizeY
	};
};
