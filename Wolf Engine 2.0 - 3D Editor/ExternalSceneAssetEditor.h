#pragma once

#include "ComponentInterface.h"
#include "EditorTypes.h"

class ExternalSceneAssetEditor : public ComponentInterface
{
public:
	static inline std::string ID = "externalSceneAssetEditor";
	std::string getId() const override { return ID; }

	ExternalSceneAssetEditor();
	ExternalSceneAssetEditor(const ExternalSceneAssetEditor&) = delete;

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}

	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void saveCustom() const override {}

	std::string getLoadingPath() const { return m_filePathParam; }

private:
	inline static const std::string TAB = "External scene";

	void onFilePathChanged();
	EditorParamString m_filePathParam = EditorParamString("File", TAB, "File", [this]() { onFilePathChanged(); }, EditorParamString::ParamStringType::FILE_EXTERNAL_SCENE);

	std::array<EditorParamInterface*, 1> m_params
	{
		&m_filePathParam
	};
};
