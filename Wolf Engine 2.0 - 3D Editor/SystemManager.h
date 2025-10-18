#pragma once

#include <FirstPersonCamera.h>
#include <WolfEngine.h>

#include "ComponentInstancier.h"
#include "DebugRenderingManager.h"
#include "EditorConfiguration.h"
#include "EditorParams.h"
#include "EntityContainer.h"
#include "GameContext.h"
#include "RayTracedWorldManager.h"
#include "RenderingPipeline.h"
#include "ResourceManager.h"

class SystemManager
{
public:
	SystemManager();
	void run();

	GameContext& getInModificationGameContext() { return m_inModificationGameContext; }

private:
	static constexpr uint32_t THREAD_COUNT_BEFORE_FRAME = 6;
	void createWolfInstance();
	void createRenderer();
	void updateBeforeFrame();

	void loadScene();
	Entity* addEntity(const std::string& filePath);
	void duplicateEntity(const Wolf::ResourceNonOwner<Entity>& entityToDuplicate, const std::string& filePath);
	void addComponent(const std::string& componentId);

	void addFakeEntities();

	void debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message) const;
	void bindUltralightCallbacks(ultralight::JSObject& jsObject);
	void resizeCallback(uint32_t width, uint32_t height) const;

	void forceCustomViewForSelectedEntity();

	// JS callbacks
	ultralight::JSValue getFrameRateJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue getVRAMRequestedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue getVRAMUsedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
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
	void takeScreenshotJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue getAllComponentTypesJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void requestEntitySelectionJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void disableEntityPickingJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void duplicateEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void editResourceJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void debugPhysicsCheckboxChangedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void onGoToEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void onRemoveEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void toggleAABBDisplayForSelectedEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue isAABBShowedForSelectedEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;
	void toggleBoundingSphereDisplayForSelectedEntity(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	ultralight::JSValue isBoundingSphereShowedForSelectedEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const;

	void selectEntity() const;
	void goToSelectedEntity() const;
	void removeSelectedEntity();
	void updateUISelectedEntity() const;

	std::unique_ptr<Wolf::WolfEngine> m_wolfInstance;
	Wolf::ResourceUniqueOwner<RenderingPipeline> m_renderer;
	Wolf::ResourceUniqueOwner<EditorConfiguration> m_configuration;
	Wolf::ResourceUniqueOwner<ResourceManager> m_resourceManager;

	/* FPS counter */
	uint32_t m_currentFramesAccumulated = 0;
	uint32_t m_stableFPS = 0;
	std::chrono::steady_clock::time_point m_startTimeFPSCounter = std::chrono::steady_clock::now();

	std::string m_currentSceneName = "Unknown scene";

	GameContext m_inModificationGameContext;
	std::mutex m_contextMutex;
	std::vector<GameContext> m_gameContexts;
	bool m_entitySelectionRequested = false;
	Wolf::ResourceUniqueOwner<EntityContainer> m_entityContainer;
	Wolf::ResourceUniqueOwner<ComponentInstancier> m_componentInstancier;
	std::unique_ptr<Wolf::FirstPersonCamera> m_camera;
	Wolf::ResourceUniqueOwner<DrawManager> m_drawManager;
	Wolf::ResourceUniqueOwner<RayTracedWorldManager> m_rayTracedWorldManager;
	bool m_rayTracedWorldBuildNeeded = false;
	Wolf::ResourceUniqueOwner<EditorPhysicsManager> m_editorPhysicsManager;

	std::unique_ptr<EditorParams> m_editorParams;

	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_selectedEntity;
	std::mutex m_entityChangedMutex;
	bool m_entityChanged = false;
	bool m_entityReloadRequested = false;
	bool m_isCameraLocked = true;
	bool m_debugPhysics = false;
	bool m_showAABBForSelectedEntity = false;
	bool m_showBoundingSphereForSelectedEntity = false;
	bool m_requestRemoveSelectedEntity = false;

	Wolf::ResourceUniqueOwner<DebugRenderingManager> m_debugRenderingManager;

	std::string m_loadSceneRequest;
	bool m_isLoading = false;
	bool m_screenshotRequested = false;
};
