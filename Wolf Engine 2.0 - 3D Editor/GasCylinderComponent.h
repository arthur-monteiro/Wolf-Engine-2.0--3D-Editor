#pragma once

#include "ComponentInterface.h"
#include "EditorTypes.h"

class GasCylinderComponent : public ComponentInterface
{
public:
	static inline std::string ID = "gasCylinderComponent";
	std::string getId() const override { return ID; }

	GasCylinderComponent(const Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager>& physicsManager);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override;
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	bool requiresInputs() const override { return false; }
	void saveCustom() const override {}

	bool isEmpty() const { return m_currentStorage == 0.0f; }
	void addShootRequest(const Wolf::Timer& globalTimer);
	void setLinkPositions(const glm::vec3& topPos, const glm::vec3& botPos);
	void onPlayerRelease(); // called when the player is not more linked to the gas cylinder

	glm::vec3 getColor() const { return m_color; }

private:
	inline static const std::string TAB = "Gas cylinder";
	Wolf::ResourceNonOwner<Wolf::Physics::PhysicsManager> m_physicsManager;

	EditorParamUInt m_maxStorage = EditorParamUInt("Max storage", TAB, "Gameplay", 1, 1000);
	EditorParamFloat m_currentStorage = EditorParamFloat("Current storage", TAB, "Gameplay", 0.0f, 1.0f);

	EditorParamVector3 m_color = EditorParamVector3("Color", TAB, "Visual", 0.0f, 1.0f);

	std::array<EditorParamInterface*, 3> m_editorParams =
	{
		&m_maxStorage,
		&m_currentStorage,
		&m_color
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
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_uniformBuffer;
};

