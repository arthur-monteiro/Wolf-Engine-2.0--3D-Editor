#pragma once

#include <WolfEngine.h>

#include "BindlessDescriptor.h"
#include "BuildingModel.h"
#include "Camera.h"
#include "EditorParams.h"
#include "GameContext.h"
#include "MainRenderingPipeline.h"
#include "ModelsContainer.h"

class SystemManager
{
public:
	SystemManager();
	void run();

private:
	void createWolfInstance();
	void createMainRenderer();
	void updateBeforeFrame();

	uint32_t addImagesToBindlessDescriptor(const std::vector<Wolf::Image*>& images) const;

	void loadScene();
	void addStaticModel(const std::string& filepath, const std::string& materialFolder, const glm::mat4& transform);
	void addBuildingModel(const std::string& filepath, const glm::mat4& transform);

	void debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message) const;
	void bindUltralightCallbacks();
	void resizeCallback(uint32_t width, uint32_t height) const;

	// JS callbacks
	ultralight::JSValue getFrameRateJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue pickFileJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue pickFolderJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue getRenderHeightJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue getRenderWidthJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue getRenderOffsetRightJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue getRenderOffsetLeftJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void setRenderOffsetLeftJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void setRenderOffsetRightJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void addModelJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void changeScaleXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeScaleYJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeScaleZJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeRotationXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeRotationYJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeRotationZJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeTranslationXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeTranslationYJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeTranslationZJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingSizeXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingSizeZJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingWindowSideSizeJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingFloorCountJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingWindowMeshJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void changeBuildingWallMeshJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void selectModelByNameJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void saveSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void loadSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void displayTypeSelectChangedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	void updateUISelectedModel();

	std::unique_ptr<Wolf::WolfEngine> m_wolfInstance;
	std::unique_ptr<MainRenderingPipeline> m_mainRenderer;

	/* FPS counter */
	uint32_t m_currentFramesAccumulated = 0;
	uint32_t m_stableFPS = 0;
	std::chrono::steady_clock::time_point m_startTimeFPSCounter = std::chrono::steady_clock::now();

	std::string m_currentSceneName = "Unknown scene";

	std::vector<GameContext> m_gameContexts;
	std::unique_ptr<ModelsContainer> m_modelsContainer;
	std::unique_ptr<Camera> m_camera;
	uint32_t m_currentBindlessOffset = 5;

	std::unique_ptr<EditorParams> m_editorParams;

	struct AddModelRequest
	{
		const std::string filepath;
		const std::string materialPath;
		const std::string type;
	};
	std::vector<AddModelRequest> m_addModelRequests;

	struct ChangeBuildingPieceMeshRequest
	{
		const uint32_t meshIdx;
		const std::string filename;
		const std::string materialFolder;
		BuildingModel::PieceType pieceType;
	};
	std::vector<ChangeBuildingPieceMeshRequest> m_changeBuildingPieceMeshRequests;

	ModelInterface* m_selectedModel = nullptr;
	bool m_isCameraLocked = false;

	std::string m_loadSceneRequest;
};

