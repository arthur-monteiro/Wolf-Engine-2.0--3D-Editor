#pragma once

#include "ComponentInterface.h"
#include "EditorTypes.h"

class ContaminationEmitter;

class ContaminationReceiver : public ComponentInterface
{
public:
	static inline std::string ID = "contaminationReceiver";
	std::string getId() const override { return ID; }

	ContaminationReceiver(std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override {}
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override;
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}

private:
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	inline static const std::string TAB = "Contamination receiver";

	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_contaminationEmitterEntity;
	void onContaminationEmitterChanged();
	EditorParamString m_contaminationEmitterParam = EditorParamString("Contamination Emitter", TAB, "General", [this]() { onContaminationEmitterChanged(); }, EditorParamString::ParamStringType::ENTITY);

	std::unordered_map<uint64_t, Wolf::ResourceUniqueOwner<Wolf::PipelineSet>> m_pipelineSetMapping;
};
