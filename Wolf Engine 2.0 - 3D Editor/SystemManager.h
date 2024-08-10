#pragma once

#include <FirstPersonCamera.h>
#include <WolfEngine.h>

#include "ComponentInstancier.h"
#include "DebugRenderingManager.h"
#include "EditorConfiguration.h"
#include "EditorParams.h"
#include "EntityContainer.h"
#include "GameContext.h"
#include "LightManager.h"
#include "RenderingPipeline.h"

class SystemManager
{
public:
	SystemManager();
	void run();

private:
	void createWolfInstance();
	void createRenderer();
	void updateBeforeFrame();

	void loadScene();
	void addEntity(const std::string& filePath);
	void addComponent(const std::string& componentId);

	void addFakeEntities();

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
	void enableEntityPickingJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void disableEntityPickingJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	void updateUISelectedEntity() const;

	std::unique_ptr<Wolf::WolfEngine> m_wolfInstance;
	Wolf::ResourceUniqueOwner<RenderingPipeline> m_renderer;
	Wolf::ResourceUniqueOwner<EditorConfiguration> m_configuration;

	/* FPS counter */
	uint32_t m_currentFramesAccumulated = 0;
	uint32_t m_stableFPS = 0;
	std::chrono::steady_clock::time_point m_startTimeFPSCounter = std::chrono::steady_clock::now();

	std::string m_currentSceneName = "Unknown scene";

	std::vector<GameContext> m_gameContexts;
	bool m_entityPickingEnabled = true;
	Wolf::ResourceUniqueOwner<EntityContainer> m_entityContainer;
	Wolf::ResourceUniqueOwner<ComponentInstancier> m_componentInstancier;
	std::unique_ptr<Wolf::FirstPersonCamera> m_camera;

	std::unique_ptr<EditorParams> m_editorParams;
	Wolf::ResourceUniqueOwner<LightManager> m_lightManager;

	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_selectedEntity;
	std::mutex m_entityChangedMutex;
	bool m_entityChanged = false;
	bool m_entityReloadRequested = false;
	bool m_isCameraLocked = true;

	Wolf::ResourceUniqueOwner<DebugRenderingManager> m_debugRenderingManager;

	std::string m_loadSceneRequest;
};
