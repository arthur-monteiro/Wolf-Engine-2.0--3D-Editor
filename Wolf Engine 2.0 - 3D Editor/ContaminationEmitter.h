#pragma once

#include "ComponentInterface.h"
#include "DescriptorSetLayoutGenerator.h"
#include "EditorConfiguration.h"
#include "EditorParamArray.h"
#include "MaterialEditor.h"
#include "Notifier.h"
#include "ShootInfo.h"

class ContaminationUpdatePass;
class RenderingPipelineInterface;

class ContaminationEmitter : public ComponentInterface
{
public:
	static inline std::string ID = "contaminationEmitter";
	std::string getId() const override { return ID; }
	static constexpr uint32_t CONTAMINATION_IDS_IMAGE_SIZE = 64;

	ContaminationEmitter(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager,
		const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);
	ContaminationEmitter(const ContaminationEmitter&) = delete;
	~ContaminationEmitter() override;

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	void updateBeforeFrame() override;
	void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}

	void addShootRequest(ShootRequest shootRequest);

	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet>& getDescriptorSet() { return m_descriptorSet; }
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout>& getDescriptorSetLayout() { return m_descriptorSetLayout; }

private:
	inline static const std::string TAB = "Contamination emitter";
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialGPUManager;
	Wolf::ResourceNonOwner<EditorConfiguration> m_editorConfiguration;
	Wolf::ResourceNonOwner<ContaminationUpdatePass> m_contaminationUpdatePass;

	class ContaminationMaterial : public ParameterGroupInterface, public Notifier
	{
	public:
		ContaminationMaterial();
		ContaminationMaterial(const ContaminationMaterial&) = delete;

		void updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialGPUManager, const Wolf::ResourceNonOwner<EditorConfiguration>& editorConfiguration);
		
		std::span<EditorParamInterface*> getAllParams() override;
		std::span<EditorParamInterface* const> getAllConstParams() const override;
		bool hasDefaultName() const override;
		uint32_t getMaterialId() const { return m_materialId; }

	private:
		inline static const std::string DEFAULT_NAME = "New material";

		std::vector<Wolf::MaterialsGPUManager::MaterialInfo> m_materialsInfo;
		Wolf::MaterialsGPUManager::MaterialCacheInfo m_materialCacheInfo;
		uint32_t m_materialId = 0;
		MaterialEditor m_materialEditor;
	};

	void onMaterialAdded();
	void onFillSceneWithValueChanged() const;
	void onParamChanged();

	void buildDebugMesh();
	void transferInfoToBuffer();

	static constexpr uint32_t MAX_MATERIALS = 8;
	EditorParamArray<ContaminationMaterial> m_contaminationMaterials = EditorParamArray<ContaminationMaterial>("Contamination materials", TAB, "Materials", MAX_MATERIALS, [this] { onMaterialAdded(); });

	EditorParamFloat m_size = EditorParamFloat("Size", TAB, "General", 1.0f, 64.0f, [this]() { onParamChanged(); });
	EditorParamVector3 m_offset = EditorParamVector3("Offset", TAB, "General", -64.0f, 64.0f, [this]() { onParamChanged(); });

	EditorParamUInt m_fillSceneWithValue = EditorParamUInt("Fill scene with value", TAB, "Tool", 0, 255, [this] { onFillSceneWithValueChanged(); }, true);
	EditorParamBool m_fillWithRandom = EditorParamBool("Fill with random", TAB, "Tool", [this] { onFillSceneWithValueChanged(); });
	EditorParamBool m_drawDebug = EditorParamBool("Draw debug", TAB, "Tool");

	std::array<EditorParamInterface*, 6> m_editorParams =
	{
		&m_contaminationMaterials,
		&m_size,
		&m_offset,
		&m_fillSceneWithValue,
		&m_fillWithRandom,
		&m_drawDebug
	};

	std::array<EditorParamInterface*, 3> m_savedEditorParams =
	{
		&m_contaminationMaterials,
		&m_size,
		&m_offset
	};

	// Graphic resources
	Wolf::ResourceUniqueOwner<Wolf::Image> m_contaminationIdsImage;
	Wolf::ResourceUniqueOwner<Wolf::Sampler> m_sampler;

	struct ContaminationInfo
	{
		glm::vec3 offset;
		float size;
		uint32_t materialOffsets[MAX_MATERIALS];
	};
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_contaminationInfoBuffer;
	bool m_transferInfoToBufferRequested = true;

	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

	// Shoot
	std::vector<ShootRequest> m_shootRequests;
	std::mutex m_shootRequestLock;

	// Debug
	bool m_debugMeshRebuildRequested = false;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_debugMesh;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_newDebugMesh; // new mesh kept here until the debug mesh is no more used
};
