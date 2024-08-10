#pragma once

#include "ComponentInterface.h"
#include "EditorTypesTemplated.h"
#include "EditorTypes.h"
#include "MaterialEditor.h"
#include "ParameterGroupInterface.h"

class Particle : public ComponentInterface, public Notifier
{
public:
	static inline std::string ID = "particleComponent";
	std::string getId() const override { return ID; }

	Particle(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	bool requiresInputs() const override { return false; }

	uint32_t getMaterialIdx() const { return m_particleMaterial.get().getMaterialIdx();	}
	uint32_t getFlipBookSizeX() const { return m_flipBookSizeX; }
	uint32_t getFlipBookSizeY() const { return m_flipBookSizeY; }

private:
	inline static const std::string TAB = "Particle";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialGPUManager;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;

	class ParticleMaterial : public ParameterGroupInterface, public Notifier
	{
	public:
		ParticleMaterial();
		ParticleMaterial(const ParticleMaterial&) = delete;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);

		std::span<EditorParamInterface*> getAllParams() override;
		std::span<EditorParamInterface* const> getAllConstParams() const override;
		bool hasDefaultName() const override;
		uint32_t getMaterialIdx() const { return m_materialIdx; }

	private:
		inline static const std::string DEFAULT_NAME = "New material";

		std::vector<Wolf::MaterialsGPUManager::MaterialInfo> m_materialsInfo;
		Wolf::MaterialsGPUManager::MaterialCacheInfo m_materialCacheInfo;
		uint32_t m_materialIdx = 0;
		MaterialEditor m_materialEditor;
	};

	EditorParamGroup<ParticleMaterial> m_particleMaterial = EditorParamGroup<ParticleMaterial>("Particle material", TAB, "Material");
	EditorParamUInt m_flipBookSizeX = EditorParamUInt("Flip book size X", TAB, "Material", 1, 8, [this]() { notifySubscribers(); });
	EditorParamUInt m_flipBookSizeY = EditorParamUInt("Flip book size Y", TAB, "Material", 1, 8, [this]() { notifySubscribers(); });

	std::array<EditorParamInterface*, 3> m_editorParams =
	{
		&m_particleMaterial,
		&m_flipBookSizeX,
		&m_flipBookSizeY
	};
};
