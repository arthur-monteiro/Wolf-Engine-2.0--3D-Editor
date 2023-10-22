#pragma once

#include <WolfEngine.h>

#include "BindlessDescriptor.h"
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

	void loadScene();
	void addStaticModel(const std::string& filepath, const std::string& materialFolder, const glm::mat4& transform);
	void addBuildingModel(const std::string& filepath, const glm::mat4& transform);

	void debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message) const;
	void bindUltralightCallbacks();
	void resizeCallback(uint32_t width, uint32_t height) const;

	// JS callbacks
	ultralight::JSValue getFrameRate(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue pickFile(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue pickFolder(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue getRenderHeight(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue getRenderWidth(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue getRenderOffsetRight(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	ultralight::JSValue getRenderOffsetLeft(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void setRenderOffsetLeft(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void setRenderOffsetRight(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void addModelJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void changeScaleX(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeScaleY(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeScaleZ(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeRotation(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeTranslation(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingSizeX(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingSizeZ(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingWindowSideSize(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingFloorCount(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void changeBuildingWindowMesh(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void selectModelByName(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void saveScene(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void loadSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

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
	std::unique_ptr<BindlessDescriptor> m_bindlessDescriptor;
	uint32_t m_currentBindlessOffset = 0;

	std::unique_ptr<EditorParams> m_editorParams;

	ModelInterface* m_selectedModel = nullptr;
	bool m_isCameraLocked = false;

	std::string m_loadSceneRequest;
};

