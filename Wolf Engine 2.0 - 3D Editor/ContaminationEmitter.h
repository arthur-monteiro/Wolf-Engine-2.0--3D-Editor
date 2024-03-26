#pragma once

#include "ComponentInterface.h"
#include "DescriptorSetLayoutGenerator.h"
#include "EditorParamArray.h"
#include "MaterialEditor.h"
#include "Notifier.h"

class ContaminationEmitter : public ComponentInterface
{
public:
	static inline std::string ID = "contaminationEmitter";
	std::string getId() const override { return ID; }

	ContaminationEmitter(const std::function<void(ComponentInterface*)>& requestReloadCallback);
	ContaminationEmitter(const ContaminationEmitter&) = delete;

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	void updateBeforeFrame() override {}
	void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet>& getDescriptorSet() { return m_descriptorSet; }
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& getDescriptorSetLayout() { return m_descriptorSetLayout; }

private:
	inline static const std::string TAB = "Contamination emitter";
	std::function<void(ComponentInterface*)> m_requestReloadCallback;

	class ContaminationMaterial : public ParameterGroupInterface, public Notifier
	{
	public:
		ContaminationMaterial();
		ContaminationMaterial(const ContaminationMaterial&) = default;
		
		std::span<EditorParamInterface*> getAllParams() override;
		std::span<EditorParamInterface* const> getAllConstParams() const override;
		bool hasDefaultName() const override;

	private:
		inline static const std::string DEFAULT_NAME = "New material";
		MaterialEditor m_material;
	};

	void onMaterialAdded();
	void onFillSceneWithValueChanged() const;
	void requestDebugMeshRebuild();
	void buildDebugMesh();

	EditorParamArray<ContaminationMaterial> m_contaminationMaterials = EditorParamArray<ContaminationMaterial>("Contamination materials", TAB, "Materials", 8, [this] { onMaterialAdded(); });

	EditorParamFloat m_size = EditorParamFloat("Size", TAB, "General", 1.0f, 64.0f, [this]() { requestDebugMeshRebuild(); });
	EditorParamVector3 m_offset = EditorParamVector3("Offset", TAB, "General", -64.0f, 64.0f, [this]() { requestDebugMeshRebuild(); });

	EditorParamUInt m_fillSceneWithValue = EditorParamUInt("Fill scene with value", TAB, "Tool", 0, 255, [this] { onFillSceneWithValueChanged(); }, true);
	EditorParamBool m_drawDebug = EditorParamBool("Draw debug", TAB, "Tool");

	std::array<EditorParamInterface*, 5> m_editorParams =
	{
		&m_contaminationMaterials,
		&m_size,
		&m_offset,
		&m_fillSceneWithValue,
		&m_drawDebug
	};

	std::array<EditorParamInterface*, 3> m_savedEditorParams =
	{
		&m_contaminationMaterials,
		& m_size,
		& m_offset
	};

	// Graphic resources
	const uint32_t CONTAMINATION_IDS_IMAGE_SIZE = 64;
	Wolf::ResourceUniqueOwner<Wolf::Image> m_contaminationIdsImage;
	Wolf::ResourceUniqueOwner<Wolf::Sampler> m_sampler;
	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

	// Debug
	bool m_debugMeshRebuildRequested = false;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_debugMesh;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_newDebugMesh; // new mesh kept here until the debug mesh is no more used
};