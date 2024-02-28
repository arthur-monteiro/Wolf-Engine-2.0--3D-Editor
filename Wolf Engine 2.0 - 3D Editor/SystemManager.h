#pragma once

#include <FirstPersonCamera.h>
#include <WolfEngine.h>

#include "ComponentInstancier.h"
#include "EditorParams.h"
#include "EntityContainer.h"
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

	void addImagesToBindlessDescriptor(const std::vector<Wolf::Image*>& images) const;

	void loadScene();
	void addEntity(const std::string& filepath) const;
	void addComponent(const std::string& filepath) const;

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
	void addEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void addComponentJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void selectEntityByNameJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void saveSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void loadSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void displayTypeSelectChangedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void openUIInBrowserJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue getAllComponentTypesJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	void updateUISelectedEntity() const;

	std::unique_ptr<Wolf::WolfEngine> m_wolfInstance;
	std::unique_ptr<MainRenderingPipeline> m_mainRenderer;

	/* FPS counter */
	uint32_t m_currentFramesAccumulated = 0;
	uint32_t m_stableFPS = 0;
	std::chrono::steady_clock::time_point m_startTimeFPSCounter = std::chrono::steady_clock::now();

	std::string m_currentSceneName = "Unknown scene";

	std::vector<GameContext> m_gameContexts;
	Wolf::ResourceUniqueOwner<EntityContainer> m_entityContainer;
	Wolf::ResourceUniqueOwner<ComponentInstancier> m_componentInstancier;
	std::unique_ptr<Wolf::FirstPersonCamera> m_camera;

	std::unique_ptr<EditorParams> m_editorParams;

	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_selectedEntity;
	bool m_isCameraLocked = false;

	std::string m_loadSceneRequest;
};