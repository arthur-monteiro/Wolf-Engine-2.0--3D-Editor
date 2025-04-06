#pragma once

#include "ComponentInterface.h"
#include "ContaminationMaterial.h"
#include "EditorTypes.h"
#include "EditorTypesTemplated.h"

class GasCylinderComponent : public ComponentInterface
{
public:
	static inline std::string ID = "gasCylinderComponent";
	std::string getId() const override { return ID; }

	GasCylinderComponent(const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback,
		const std::function<void(ComponentInterface*)>& requestReloadCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override;
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void saveCustom() const override {}

	bool isEmpty() const { return m_currentStorage == 0.0f; }
	void addShootRequest(const Wolf::Timer& globalTimer);
	void setLinkPositions(const glm::vec3& topPos, const glm::vec3& botPos);
	void onPlayerRelease(); // called when the player is not more linked to the gas cylinder

	const glm::vec3& getColor() const { return m_color; }
	uint32_t getCleanFlags() const { return m_cleanFlags; }

private:
	inline static const std::string TAB = "Gas cylinder";
	Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager> m_physicsManager;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;

	EditorParamUInt m_maxStorage = EditorParamUInt("Max storage", TAB, "Gameplay", 1, 1000);
	EditorParamFloat m_currentStorage = EditorParamFloat("Current storage", TAB, "Gameplay", 0.0f, 1.0f);

	static constexpr uint32_t MAX_MATERIALS = 8;
	void onMaterialAdded();
	void onMaterialChanged();
	EditorParamArray<ContaminationMaterialArrayItem<TAB>> m_contaminationMaterials = EditorParamArray<ContaminationMaterialArrayItem<TAB>>("Contamination materials", TAB, "Materials", MAX_MATERIALS, [this] { onMaterialAdded(); });

	std::array<EditorParamInterface*, 3> m_editorParams =
	{
		&m_maxStorage,
		&m_currentStorage,
		&m_contaminationMaterials,
	};

	std::unordered_map<uint64_t, Wolf::ResourceUniqueOwner<Wolf::PipelineSet>> m_pipelineSetMapping;

	// GPU resources
	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

	struct UniformData
	{
		glm::vec3 color;
		float currentValue;

		float minY;
		float maxY;
		glm::vec2 padding;
	};
	Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;

	// Computed info
	glm::vec3 m_color;
	uint32_t m_cleanFlags;
};

