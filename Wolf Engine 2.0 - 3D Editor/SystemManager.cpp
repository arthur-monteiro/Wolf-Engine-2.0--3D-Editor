#include "SystemManager.h"

#include <ctime>
#include <chrono>
#include <fstream>
#include <iostream>

#include <GPUMemoryDebug.h>
#include <JSONReader.h>
#include <ProfilerCommon.h>

#include "GraphicSettingsFakeEntity.h"
#include "MaterialListFakeEntity.h"
#include "RuntimeContext.h"

SystemManager::SystemManager()
{
	m_configuration.reset(new EditorConfiguration("config/editor.ini"));
	m_editorParams.reset(new EditorParams(1920, 1080));

	createWolfInstance();

	m_editorParams->setWindowWidth(Wolf::g_configuration->getWindowWidth());
	m_editorParams->setWindowHeight(Wolf::g_configuration->getWindowHeight());

	m_camera.reset(new Wolf::FirstPersonCamera(glm::vec3(1.4f, 1.2f, 0.3f), glm::vec3(2.0f, 0.9f, -0.3f), glm::vec3(0.0f, 1.0f, 0.0f), 0.01f, 20.0f, 16.0f / 9.0f));

	if (g_editorConfiguration->getEnableRayTracing())
	{
		m_rayTracedWorldManager.reset(new RayTracedWorldManager);
	}

	createRenderer();

	std::function<void(ComponentInterface*)> requestReloadCallback = [this](ComponentInterface* component)
		{
			if (m_selectedEntity && component->isOnEntity(*m_selectedEntity))
				m_entityReloadRequested = true;
		};

	m_resourceManager.reset(new ResourceManager([this](const std::string& resourceName, const std::string& iconPath, ResourceManager::ResourceId resourceId)
		{
			m_wolfInstance->evaluateUserInterfaceScript("addResourceToList(\"" + resourceName + "\", \"" + iconPath + "\", \"" + std::to_string(resourceId) + "\");");
		}, [this](const std::string& resourceName, const std::string& iconPath, ResourceManager::ResourceId resourceId)
		{
			m_wolfInstance->evaluateUserInterfaceScript("updateResource(\"" + resourceName + "\", \"" + iconPath + "\", \"" + std::to_string(resourceId) + "\");");
		}
		, m_wolfInstance->getMaterialsManager(), m_renderer.createNonOwnerResource<RenderingPipelineInterface>(), requestReloadCallback, m_configuration.createNonOwnerResource()));
	
	m_entityContainer.reset(new EntityContainer);
	m_componentInstancier.reset(new ComponentInstancier(m_wolfInstance->getMaterialsManager(), m_renderer.createNonOwnerResource<RenderingPipelineInterface>(), requestReloadCallback, 
		[this](const std::string& entityLoadingPath)
		{
			std::vector<Wolf::ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
			for (Wolf::ResourceUniqueOwner<Entity>& entity : allEntities)
			{
				if (!entity->isFake() && entity->getLoadingPath() == entityLoadingPath)
					return entity.createNonOwnerResource();
			}
			Wolf::Debug::sendCriticalError("Entity not found");
			return allEntities[0].createNonOwnerResource();
		},
		m_configuration.createNonOwnerResource(),
		m_resourceManager.createNonOwnerResource(),
		m_wolfInstance->getPhysicsManager(),
		m_entityContainer.createNonOwnerResource()));
	
	if (!m_configuration->getDefaultScene().empty())
	{
		m_loadSceneRequest = m_configuration->getDefaultScene();
	}

	m_debugRenderingManager.reset(new DebugRenderingManager);
	m_drawManager.reset(new DrawManager(m_wolfInstance->getRenderMeshList(), m_renderer.createNonOwnerResource<RenderingPipelineInterface>()));
	m_editorPhysicsManager.reset(new EditorPhysicsManager(m_wolfInstance->getPhysicsManager()));

	addFakeEntities();
}

void SystemManager::run()
{
	while (!m_wolfInstance->windowShouldClose() /* check if the window should close (for example if the user pressed alt+f4)*/)
	{
		bool doScreenShot = false;
		if (g_editorConfiguration->getTakeScreenshotAfterFrameCount() != 0 && Wolf::g_runtimeContext->getCurrentCPUFrameNumber() == g_editorConfiguration->getTakeScreenshotAfterFrameCount())
		{
			doScreenShot = true;
		}

		updateBeforeFrame();
		m_renderer->frame(m_wolfInstance.get(), doScreenShot);

		/* Update FPS counter */
		const auto currentTime = std::chrono::steady_clock::now();
		m_currentFramesAccumulated++;
		const long long durationInMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_startTimeFPSCounter).count();
		if (durationInMs > 1000)
		{
			m_stableFPS = static_cast<uint32_t>(std::round((1000.0f * static_cast<float>(m_currentFramesAccumulated)) / static_cast<float>(durationInMs)));

			m_currentFramesAccumulated = 0;
			m_startTimeFPSCounter = currentTime;
		}

		FrameMark;

		if (doScreenShot)
			break;
	}

	m_wolfInstance->waitIdle();

	m_renderer->clear();
	m_wolfInstance->getRenderMeshList()->clear();
	m_drawManager->clear();
	m_debugRenderingManager->clearAll();
	m_selectedEntity.reset(nullptr);
	m_entityContainer->clear();
}

void SystemManager::createWolfInstance()
{
	Wolf::WolfInstanceCreateInfo wolfInstanceCreateInfo;
	wolfInstanceCreateInfo.configFilename = "config/config.ini";
	wolfInstanceCreateInfo.debugCallback = std::bind(&SystemManager::debugCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	wolfInstanceCreateInfo.resizeCallback = [this](uint32_t width, uint32_t height)
	{
		resizeCallback(width, height);
	};
	wolfInstanceCreateInfo.htmlURL = "UI/UI.html";
	wolfInstanceCreateInfo.bindUltralightCallbacks = [this](ultralight::JSObject& jsObject) { bindUltralightCallbacks(jsObject); };
	wolfInstanceCreateInfo.useBindlessDescriptor = true;
	wolfInstanceCreateInfo.threadCountBeforeFrameAndRecord = THREAD_COUNT_BEFORE_FRAME;

	m_wolfInstance.reset(new Wolf::WolfEngine(wolfInstanceCreateInfo));

	m_gameContexts.reserve(Wolf::g_configuration->getMaxCachedFrames());
	std::vector<void*> contextPtrs(Wolf::g_configuration->getMaxCachedFrames());
	for (uint32_t i = 0; i < Wolf::g_configuration->getMaxCachedFrames(); ++i)
	{
		m_gameContexts.emplace_back();
		contextPtrs[i] = &m_gameContexts.back();
	}
	m_wolfInstance->setGameContexts(contextPtrs);

	EditorParamInterface::setGlobalWolfInstance(m_wolfInstance.get());
}

void SystemManager::createRenderer()
{
	Wolf::NullableResourceNonOwner<RayTracedWorldManager> rayTracedWorldManager;
	if (m_rayTracedWorldManager)
	{
		rayTracedWorldManager = m_rayTracedWorldManager.createNonOwnerResource();
	}
	m_renderer.reset(new RenderingPipeline(m_wolfInstance.get(), m_editorParams.get(), rayTracedWorldManager));

	m_renderer->getSkyBoxManager()->setOnCubeMapChangedCallback([this](Wolf::ResourceUniqueOwner<Wolf::Image>& image) { m_wolfInstance->getLightManager()->setSkyCubeMap(image.createNonOwnerResource()); });
	m_wolfInstance->getLightManager()->setSkyCubeMap(m_renderer->getSkyBoxManager()->getCubeMapImage().createNonOwnerResource());
}

void SystemManager::debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message) const
{
	switch (severity)
	{
	case Wolf::Debug::Severity::ERROR:
		std::cout << "Error : ";
		break;
	case Wolf::Debug::Severity::WARNING:
		std::cout << "Warning : ";
		break;
	case Wolf::Debug::Severity::INFO:
		std::cout << "Info : ";
		break;
	case Wolf::Debug::Severity::VERBOSE:
		return;
	}

	std::cout << message << '\n';

	if (!m_isLoading && m_configuration && m_configuration->getDisplayLogsToUI())
	{
		std::string escapedMessage;
		for (const char character : message)
		{
			if (character == '\\')
				escapedMessage += "\\\\";
			else if (character == '\"')
				escapedMessage += "\\\"";
			else if (character == '\n')
				escapedMessage += "  ";
			else
				escapedMessage += character;
		}

		if (m_wolfInstance)
		{
			switch (severity)
			{
			case Wolf::Debug::Severity::ERROR:
				m_wolfInstance->evaluateUserInterfaceScript("addLog(\"" + escapedMessage + R"(", "logError"))");
				break;
			case Wolf::Debug::Severity::WARNING:
				m_wolfInstance->evaluateUserInterfaceScript("addLog(\"" + escapedMessage + R"(", "logWarning"))");
				break;
			case Wolf::Debug::Severity::INFO:
				m_wolfInstance->evaluateUserInterfaceScript("addLog(\"" + escapedMessage + R"(", "logInfo"))");
				break;
			case Wolf::Debug::Severity::VERBOSE:
				return;
			}
		}
	}
}

void SystemManager::bindUltralightCallbacks(ultralight::JSObject& jsObject)
{
	jsObject["getFrameRate"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getFrameRateJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getVRAMRequested"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getVRAMRequestedJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getVRAMUsed"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getVRAMUsedJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["addEntity"] = std::bind(&SystemManager::addEntityJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["addComponent"] = std::bind(&SystemManager::addComponentJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["pickFile"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::pickFileJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderHeight"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderHeightJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderWidth"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderWidthJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderOffsetLeft"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderOffsetLeftJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["setRenderOffsetLeft"] = std::bind(&SystemManager::setRenderOffsetLeftJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["getRenderOffsetRight"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderOffsetRightJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["setRenderOffsetRight"] = std::bind(&SystemManager::setRenderOffsetRightJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["selectEntityByName"] = std::bind(&SystemManager::selectEntityByNameJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["saveScene"] = std::bind(&SystemManager::saveSceneJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["loadScene"] = std::bind(&SystemManager::loadSceneJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["displayTypeSelectChanged"] = std::bind(&SystemManager::displayTypeSelectChangedJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["openUIInBrowser"] = std::bind(&SystemManager::openUIInBrowserJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["getAllComponentTypes"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getAllComponentTypesJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["requestEntitySelection"] = std::bind(&SystemManager::requestEntitySelectionJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["duplicateEntity"] = std::bind(&SystemManager::duplicateEntityJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["editResource"] = std::bind(&SystemManager::editResourceJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["debugPhysicsCheckboxChanged"] = std::bind(&SystemManager::debugPhysicsCheckboxChangedJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["goToEntity"] = std::bind(&SystemManager::onGoToEntityJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["removeEntity"] = std::bind(&SystemManager::onRemoveEntityJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["toggleAABBDisplayForSelectedEntity"] = std::bind(&SystemManager::toggleAABBDisplayForSelectedEntityJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["isAABBShowedForSelectedEntity"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::isAABBShowedForSelectedEntityJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["toggleBoundingSphereDisplayForSelectedEntity"] = std::bind(&SystemManager::toggleBoundingSphereDisplayForSelectedEntity, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["isBoundingSphereShowedForSelectedEntity"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::isBoundingSphereShowedForSelectedEntityJSCallback, this, std::placeholders::_1, std::placeholders::_2));
}

void SystemManager::resizeCallback(uint32_t width, uint32_t height) const
{
	m_editorParams->setWindowWidth(width);
	m_editorParams->setWindowHeight(height);

	m_wolfInstance->evaluateUserInterfaceScript("refreshWindowSize()");
}

ultralight::JSValue SystemManager::getFrameRateJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string fpsStr = "FPS: " + std::to_string(m_stableFPS);
	return { fpsStr.c_str() };
}

ultralight::JSValue SystemManager::getVRAMRequestedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string vramRequestedStr = std::to_string(Wolf::GPUMemoryDebug::getTotalMemoryRequested() / (static_cast<uint64_t>(1024u) * 1024u)) + " MB";
	return { vramRequestedStr.c_str() };
}

ultralight::JSValue SystemManager::getVRAMUsedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string vramUsedStr = std::to_string(Wolf::GPUMemoryDebug::getTotalMemoryRequested() / (static_cast<uint64_t>(1024u) * 1024u)) + " MB";
	return { vramUsedStr.c_str() };
}

std::string exec(const char* cmd)
{
	std::array<char, 128> buffer{};
	std::string result;
	FILE* pipe = _popen(cmd, "r");
	if (!pipe)
		Wolf::Debug::sendError("Can't open " + std::string(cmd));
	
	while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) 
	{
		result += buffer.data();
	}

	if(_pclose(pipe) < 0)
	{
		Wolf::Debug::sendError("Error with command " + std::string(cmd) + ": " + result);
		return "";
	}

	return result.substr(0, result.size() -1); // remove \n
}

ultralight::JSValue SystemManager::pickFileJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string inputOption = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	const std::string inputFilter = static_cast<ultralight::String>(args[1].ToString()).utf8().data();

	std::string fullFilePath;
#ifdef _WIN32
	const std::string cmd = "BrowseToFile.exe " + inputOption + " " + inputFilter + ' ' + m_configuration->getDataFolderPath();
	fullFilePath = exec(cmd.c_str());
#else
	Debug::sendError("Pick file not implemented for this platform");
#endif

	fullFilePath = m_configuration->computeLocalPathFromFullPath(fullFilePath);

	return fullFilePath.c_str();
}

ultralight::JSValue SystemManager::pickFolderJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	// Not implemented
	__debugbreak();

	return "";
}

ultralight::JSValue SystemManager::getRenderHeightJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderHeight());
	return r.c_str();
}

ultralight::JSValue SystemManager::getRenderWidthJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderWidth());
	return r.c_str();
}

ultralight::JSValue SystemManager::getRenderOffsetRightJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderOffsetRight());
	return r.c_str();
}

ultralight::JSValue SystemManager::getRenderOffsetLeftJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderOffsetLeft());
	return r.c_str();
}

void SystemManager::setRenderOffsetLeftJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const uint32_t value = static_cast<uint32_t>(args[0].ToNumber());
	m_editorParams->setRenderOffsetLeft(value);
}

void SystemManager::setRenderOffsetRightJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const uint32_t value = static_cast<uint32_t>(args[0].ToNumber());
	m_editorParams->setRenderOffsetRight(value);
}

void SystemManager::addEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string filepath = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	addEntity(filepath);
}

void SystemManager::addComponentJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string componentId = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	addComponent(componentId);
}

void SystemManager::selectEntityByNameJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string name = static_cast<ultralight::String>(args[0].ToString()).utf8().data();

	std::vector<Wolf::ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
	for (Wolf::ResourceUniqueOwner<Entity>& entity : allEntities)
	{
		if(entity->getName() == name)
		{
			m_selectedEntity.reset(new Wolf::ResourceNonOwner<Entity>(entity.createNonOwnerResource()));
			selectEntity();
			break;
		}
	}
}

void SystemManager::saveSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string& outputFilePath = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	const std::string& sceneName = static_cast<ultralight::String>(args[1].ToString()).utf8().data();
	m_currentSceneName = sceneName;
	m_wolfInstance->evaluateUserInterfaceScript("setSceneName(\"" + m_currentSceneName + "\")");

	std::ofstream outputFile(m_configuration->computeFullPathFromLocalPath(outputFilePath));

	// Header comments
	const time_t now = time(nullptr);
	tm newTime;
	localtime_s(&newTime, &now);
	outputFile << "// Save scene: " << newTime.tm_mday << "/" << 1 + newTime.tm_mon << "/" << 1900 + newTime.tm_year << " " << newTime.tm_hour << ":" << newTime.tm_min << "\n\n";

	outputFile << "{\n";

	// Scene data
	outputFile << "\t\"sceneName\":\"" << m_currentSceneName << "\",\n";

	// Camera
	outputFile << "\t\"defaultCamera\": {\n";
	outputFile << "\t\t\"posX\": " << m_camera->getPosition().x << ",\n";
	outputFile << "\t\t\"posY\": " << m_camera->getPosition().y << ",\n";
	outputFile << "\t\t\"posZ\": " << m_camera->getPosition().z << ",\n";
	outputFile << "\t\t\"phi\": " << m_camera->getPhi() << ",\n";
	outputFile << "\t\t\"theta\": " << m_camera->getTheta() << "\n";
	outputFile << "\t},\n";

	const std::vector<Wolf::ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
	uint32_t entityCountToSave = 0;
	for (const Wolf::ResourceUniqueOwner<Entity>& entity : allEntities)
	{
		if (!entity->isFake())
		{
			entityCountToSave++;
		}
	}
	outputFile << "\t\"entityCount\":" << entityCountToSave << ",\n";

	// Model infos
	outputFile << "\t\"entities\": [\n";
	for (uint32_t i = 0; i < allEntities.size(); ++i)
	{
		const Wolf::ResourceUniqueOwner<Entity>& entity = allEntities[i];
		if (entity->isFake())
			continue;

		entity->save();

		outputFile << "\t\t{\n";

		outputFile << "\t\t\t\"loadingPath\":\"" << entity->computeEscapedLoadingPath() << "\"\n";

		outputFile << "\t\t}";
		if (i != allEntities.size() - 1)
		{
			outputFile << ",";
		}
		outputFile << "\n";
	}
	outputFile << "\t]\n";

	outputFile << "}\n";

	outputFile.close();

	m_resourceManager->save();

	Wolf::Debug::sendInfo("Save successful!");
}

void SystemManager::loadSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string filePath = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	m_loadSceneRequest = filePath;
}

void SystemManager::displayTypeSelectChangedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	m_contextMutex.lock();

	const std::string displayType = static_cast<ultralight::String>(args[0].ToString()).utf8().data();

	if (displayType == "albedo")
		m_inModificationGameContext.displayType = GameContext::DisplayType::ALBEDO;
	else if (displayType == "normal")
		m_inModificationGameContext.displayType = GameContext::DisplayType::NORMAL;
	else if (displayType == "roughness")
		m_inModificationGameContext.displayType = GameContext::DisplayType::ROUGHNESS;
	else if (displayType == "metalness")
		m_inModificationGameContext.displayType = GameContext::DisplayType::METALNESS;
	else if (displayType == "matAO")
		m_inModificationGameContext.displayType = GameContext::DisplayType::MAT_AO;
	else if (displayType == "anisoStrength")
		m_inModificationGameContext.displayType = GameContext::DisplayType::ANISO_STRENGTH;
	else if (displayType == "lighting")
		m_inModificationGameContext.displayType = GameContext::DisplayType::LIGHTING;
	else if (displayType == "entityIdx")
		m_inModificationGameContext.displayType = GameContext::DisplayType::ENTITY_IDX;
	else if (displayType == "rayTracedWorldDebugAlbedo")
	{
		if (!g_editorConfiguration->getEnableRayTracing())
		{
			Wolf::Debug::sendError("Can't display ray traced debug because ray tracing is not enabled");
		}
		else
		{
			m_inModificationGameContext.displayType = GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_ALBEDO;
		}
	}
	else if (displayType == "rayTracedWorldDebugInstanceId")
	{
		if (!g_editorConfiguration->getEnableRayTracing())
		{
			Wolf::Debug::sendError("Can't display ray traced debug because ray tracing is not enabled");
		}
		else
		{
			m_inModificationGameContext.displayType = GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_INSTANCE_ID;
		}
	}
	else if (displayType == "rayTracedWorldDebugPrimitiveId")
	{
		if (!g_editorConfiguration->getEnableRayTracing())
		{
			Wolf::Debug::sendError("Can't display ray traced debug because ray tracing is not enabled");
		}
		else
		{
			m_inModificationGameContext.displayType = GameContext::DisplayType::RAY_TRACED_WORLD_DEBUG_PRIMITIVE_ID;
		}
	}
	else if (displayType == "pathTracing")
	{
		if (!g_editorConfiguration->getEnableRayTracing())
		{
			Wolf::Debug::sendError("Can't display path tracing because ray tracing is not enabled");
		}
		else
		{
			m_inModificationGameContext.displayType = GameContext::DisplayType::PATH_TRACING;
		}
	}
	else
		Wolf::Debug::sendError("Unsupported display type: " + displayType);

	m_contextMutex.unlock();
}

void SystemManager::openUIInBrowserJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	if(!Wolf::g_configuration->getUICommands())
		Wolf::Debug::sendWarning("UI Commands are not saved");

	std::ifstream inHTML("UI/UI.html");

	std::filesystem::remove_all("tmp/UI/");
	std::filesystem::create_directories("tmp/UI/");
	std::ofstream outHTML("tmp/UI/UI_dmp.html");

	std::string inLine;
	while (getline(inHTML, inLine))
	{
		outHTML << inLine << "\n";

		if (inLine.find("<body>") != std::string::npos)
		{
			std::erase_if(inLine, isspace);
			if (inLine != "<body>")
				Wolf::Debug::sendError("Body begin declaration is not written on a dedicated line, this behaviour is not supported");

			outHTML << "<script type=\"text/javascript\">\n";
			outHTML << "\twindow.addEventListener('DOMContentLoaded', (event) => {\n";

			outHTML << "if (navigator.userAgent.indexOf('Ultralight') != -1) {\n";
			outHTML << "}\n";
			outHTML << "else {\n";
			outHTML << "document.getElementsByTagName(\"body\")[0].style.backgroundColor = \"#191919\"; \n";

			outHTML << "let script = document.createElement(\"script\");\n";
			outHTML << "script.src = \"./fakeEngine.js\"; \n";
			outHTML << "document.head.appendChild(script); \n";

			for (const std::string& command : m_wolfInstance->getSavedUICommands())
			{
				if (command.find("setCameraPosition") != std::string::npos || command.find("refreshWindowSize") != std::string::npos)
					continue;

				outHTML << "\n// STEP\n";
				outHTML << command << "\n";
			}

			outHTML << "}\n";

			outHTML << "\t});\n";
			outHTML << "</script>";
		}
	}
	outHTML.close();
	inHTML.close();

	std::filesystem::copy("UI/", "tmp/UI/", std::filesystem::copy_options::recursive);
	system("start tmp/UI/UI_dmp.html");
}

ultralight::JSValue SystemManager::getAllComponentTypesJSCallback(const ultralight::JSObject& thisObject,
                                                                  const ultralight::JSArgs& args)
{
	if (m_selectedEntity)
		return { m_componentInstancier->getAllComponentTypes(*m_selectedEntity).c_str() };
	else
		return R"({ "components": [ ] })";
}

void SystemManager::requestEntitySelectionJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	m_entitySelectionRequested = true;
}

void SystemManager::duplicateEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string previousEntityName = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	const std::string filepath = static_cast<ultralight::String>(args[1].ToString()).utf8().data();

	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> entityToDuplicate;

	std::vector<Wolf::ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
	for (Wolf::ResourceUniqueOwner<Entity>& entity : allEntities)
	{
		if (entity->getName() == previousEntityName)
		{
			entityToDuplicate.reset(new Wolf::ResourceNonOwner<Entity>(entity.createNonOwnerResource()));
			break;
		}
	}

	if (entityToDuplicate)
	{
		duplicateEntity(*entityToDuplicate, filepath);
	}
	else
	{
		Wolf::Debug::sendError("Entity to duplicate not found");
	}
}

void SystemManager::editResourceJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string resourceIdStr = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	ResourceManager::ResourceId resourceId = static_cast<ResourceManager::ResourceId>(std::stoi(resourceIdStr));

	m_selectedEntity.reset(nullptr);
	Wolf::ResourceNonOwner<Entity> entity = m_resourceManager->computeResourceEditor(resourceId);
	m_selectedEntity.reset(new Wolf::ResourceNonOwner<Entity>(entity));

	updateUISelectedEntity();
}

void SystemManager::debugPhysicsCheckboxChangedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const bool checked = args[0].ToBoolean();
	m_debugPhysics = checked;
}

void SystemManager::onGoToEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	selectEntityByNameJSCallback(thisObject, args);
	goToSelectedEntity();
}

void SystemManager::onRemoveEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	selectEntityByNameJSCallback(thisObject, args);
	m_requestRemoveSelectedEntity = true;
}

void SystemManager::toggleAABBDisplayForSelectedEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	m_showAABBForSelectedEntity = !m_showAABBForSelectedEntity;
}

ultralight::JSValue SystemManager::isAABBShowedForSelectedEntityJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	return m_showAABBForSelectedEntity ? "true" : "false";
}

void SystemManager::toggleBoundingSphereDisplayForSelectedEntity(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	m_showBoundingSphereForSelectedEntity = !m_showBoundingSphereForSelectedEntity;
}

ultralight::JSValue SystemManager::isBoundingSphereShowedForSelectedEntityJSCallback(
	const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	return m_showBoundingSphereForSelectedEntity ? "true" : "false";
}

void SystemManager::selectEntity() const
{
	updateUISelectedEntity();
}

void SystemManager::goToSelectedEntity() const
{
	if ((*m_selectedEntity)->hasModelComponent())
	{
		Wolf::AABB entityAABB = (*m_selectedEntity)->getAABB();
		float entityHeight = entityAABB.getMax().y - entityAABB.getMin().y;

		m_camera->setPosition(entityAABB.getCenter() + glm::vec3(-entityHeight, entityHeight, -entityHeight));
		m_camera->setPhi(-0.645398319f);
		m_camera->setTheta(glm::quarter_pi<float>());
	}
}

void SystemManager::removeSelectedEntity()
{
	Wolf::ResourceNonOwner<Entity>* selectedEntity = m_selectedEntity.release();
	m_entityContainer->removeEntity(selectedEntity->operator->());
	delete selectedEntity;

	m_entityChanged = true; // this is a way to reload entity list
}

void SystemManager::updateBeforeFrame()
{
	PROFILE_FUNCTION

	if(!m_loadSceneRequest.empty())
	{
		loadScene();
	}

	// Context update
	{
		const uint32_t contextId = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();
		if (m_contextMutex.try_lock())
		{
			m_gameContexts[contextId] = m_inModificationGameContext;
			m_contextMutex.unlock();
		}
		else
		{
			m_gameContexts[contextId] = m_gameContexts[(contextId + (Wolf::g_configuration->getMaxCachedFrames() - 1)) % Wolf::g_configuration->getMaxCachedFrames()];
		}
	}

	m_debugRenderingManager->clearBeforeFrame();

	if (m_requestRemoveSelectedEntity)
	{
		removeSelectedEntity();
		m_requestRemoveSelectedEntity = false;
	}

	m_entityContainer->moveToNextFrame([this](const std::string& componentId)
		{
			return m_componentInstancier->instanciateComponent(componentId);
		});

	m_entityChangedMutex.lock();
	if (m_entityChanged)
	{
		m_wolfInstance->evaluateUserInterfaceScript("resetEntityList()");
		const std::vector<Wolf::ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
		for (const Wolf::ResourceUniqueOwner<Entity>& entity : allEntities)
		{
			m_wolfInstance->evaluateUserInterfaceScript("addEntityToList(\"" + entity->getName() + "\", \"" + entity->computeEscapedLoadingPath() + "\"," + std::to_string(entity->isFake()) + ")");
		}
		m_entityChanged = false;
	}
	m_entityChangedMutex.unlock();

	if (m_entityReloadRequested)
	{
		if (m_selectedEntity)
			updateUISelectedEntity();
		m_entityReloadRequested = false;
	}

	if (m_selectedEntity && (*m_selectedEntity)->hasModelComponent())
	{
		if (m_showAABBForSelectedEntity)
		{
			m_debugRenderingManager->addAABB((*m_selectedEntity)->getAABB());
		}

		if (m_showBoundingSphereForSelectedEntity)
		{
			Wolf::BoundingSphere boundingSphere = (*m_selectedEntity)->getBoundingSphere();
			m_debugRenderingManager->addWiredSphere(boundingSphere.getCenter(), boundingSphere.getRadius(), glm::vec3(1.0f));
		}
	}

	m_resourceManager->updateBeforeFrame();

	std::vector<Wolf::ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();

	Wolf::ResourceNonOwner<Wolf::RenderMeshList> renderList = m_wolfInstance->getRenderMeshList();
	Wolf::ResourceNonOwner<Wolf::LightManager> lightManager = m_wolfInstance->getLightManager().createNonOwnerResource();
	DebugRenderingManager& debugRenderingManager = *m_debugRenderingManager;

	if (m_debugPhysics)
	{
		m_editorPhysicsManager->addDebugInfo(debugRenderingManager);
	}

	m_wolfInstance->getCameraList().addCameraForThisFrame(m_camera.get(), 0);
	m_camera->setAspect(m_editorParams->getAspect());

	m_renderer->update(m_wolfInstance.get());

	Wolf::ResourceNonOwner<DrawManager> drawManager = m_drawManager.createNonOwnerResource();
	Wolf::ResourceNonOwner<EditorPhysicsManager> editorPhysicsManager= m_editorPhysicsManager.createNonOwnerResource();
	Wolf::ResourceNonOwner<Wolf::InputHandler> inputHandler = m_wolfInstance->getInputHandler();
	const Wolf::Timer& globalTimer = m_wolfInstance->getGlobalTimer();

	uint32_t startRange = 0;
	uint32_t elementCountPerThread = static_cast<uint32_t>(allEntities.size()) / THREAD_COUNT_BEFORE_FRAME;
	for (uint32_t i = 0; i < THREAD_COUNT_BEFORE_FRAME; ++i)
	{
		uint32_t endRange = startRange + elementCountPerThread;
		if (i == THREAD_COUNT_BEFORE_FRAME - 1)
			endRange = static_cast<uint32_t>(allEntities.size());
		m_wolfInstance->addJobBeforeFrame([&allEntities, startRange, endRange, inputHandler, &globalTimer, drawManager, editorPhysicsManager, &debugRenderingManager, lightManager]()
			{
				for (uint32_t entityIdx = startRange; entityIdx < endRange; ++entityIdx)
				{
					allEntities[entityIdx]->updateBeforeFrame(inputHandler, globalTimer, drawManager, editorPhysicsManager);
					allEntities[entityIdx]->addDebugInfo(debugRenderingManager);
					allEntities[entityIdx]->addLightToLightManager(lightManager);
				}
			});

		startRange += elementCountPerThread;
	}

	m_wolfInstance->addJobBeforeFrame([this, renderList]() { m_debugRenderingManager->addMeshesToRenderList(renderList); }, true);

	m_wolfInstance->addJobBeforeFrame([this, &allEntities]()
	{
		if (m_rayTracedWorldBuildNeeded)
		{
			RayTracedWorldManager::RayTracedWorldInfo tlasInfo;

			bool errorEncountered = false;
			for (Wolf::ResourceUniqueOwner<Entity>& entity : allEntities)
			{
				if (!entity->getInstancesForRayTracedWorld(tlasInfo.m_instances))
				{
					errorEncountered = true;
					break;
				}
			}

			if (!tlasInfo.m_instances.empty() && !errorEncountered)
			{
				m_rayTracedWorldManager->build(tlasInfo);
				m_rayTracedWorldBuildNeeded = false;
			}
		}
	}, true);

	m_wolfInstance->updateBeforeFrame();

	if (inputHandler->keyPressedThisFrame(GLFW_KEY_ESCAPE))
	{
		m_isCameraLocked = !m_isCameraLocked;
		m_camera->setLocked(m_isCameraLocked);
	}

	// Select object by click
	if (m_entitySelectionRequested && inputHandler->mouseButtonPressedThisFrame(GLFW_MOUSE_BUTTON_LEFT))
	{
		float currentMousePosX, currentMousePosY;
		inputHandler->getMousePosition(currentMousePosX, currentMousePosY);
		m_renderer->requestPixelId(static_cast<uint32_t>(currentMousePosX), static_cast<uint32_t>(currentMousePosY), [this](uint32_t entityIdx)
		{
			if (entityIdx != -1)
			{
				m_selectedEntity.reset(new Wolf::ResourceNonOwner<Entity>(m_entityContainer->getEntities()[entityIdx].createNonOwnerResource()));
				updateUISelectedEntity();
			}

			m_wolfInstance->evaluateUserInterfaceScript("finishEntitySelection();");
		});

		m_entitySelectionRequested = false;
	}

	const glm::vec3 cameraPosition = m_camera->getPosition();
	m_wolfInstance->evaluateUserInterfaceScript("setCameraPosition(\"" + std::to_string(cameraPosition.x) + ", " + std::to_string(cameraPosition.y) + ", " + std::to_string(cameraPosition.z) + "\")");

	// Don't reset earlier to avoid displaying all resources warnings to the UI
	if (m_isLoading)
	{
		m_isLoading = false;
	}
}

void SystemManager::loadScene()
{
	m_isLoading = true;

	m_wolfInstance->getRenderMeshList()->clear();
	m_debugRenderingManager->clearBeforeFrame();
	m_wolfInstance->waitIdle();

	m_selectedEntity.reset(nullptr);
	m_entityContainer->clear();

	m_resourceManager->clear();
	m_drawManager->clear();

	m_wolfInstance->evaluateUserInterfaceScript("resetEntityList()");
	m_wolfInstance->evaluateUserInterfaceScript("resetSelectedEntity()");

	addFakeEntities();

	Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { m_configuration->computeFullPathFromLocalPath(m_loadSceneRequest) });

	const std::string& sceneName = jsonReader.getRoot()->getPropertyString("sceneName");
	m_inModificationGameContext.currentSceneName = sceneName;

	// Camera
	if (Wolf::JSONReader::JSONObjectInterface* cameraObject = jsonReader.getRoot()->getPropertyObject("defaultCamera"))
	{
		float posX = cameraObject->getPropertyFloat("posX");
		float posY = cameraObject->getPropertyFloat("posY");
		float posZ = cameraObject->getPropertyFloat("posZ");

		float phi = cameraObject->getPropertyFloat("phi");
		float theta = cameraObject->getPropertyFloat("theta");

		m_camera->setPosition(glm::vec3(posX, posY, posZ));
		m_camera->setPhi(phi);
		m_camera->setTheta(theta);
	}

	const uint32_t entityCount = static_cast<uint32_t>(jsonReader.getRoot()->getPropertyFloat("entityCount"));
	for(uint32_t entityIdx = 0; entityIdx < entityCount; entityIdx++)
	{
		Wolf::JSONReader::JSONObjectInterface* entityObject = jsonReader.getRoot()->getArrayObjectItem("entities", entityIdx);

		const std::string& entityLoadingPath = entityObject->getPropertyString("loadingPath");
		std::string deduplicatedLoadingPath;
		bool ignoreNextCharacter = false;
		for (const char character : entityLoadingPath)
		{
			if (ignoreNextCharacter)
			{
				ignoreNextCharacter = false;
				continue;
			}

			if (character == '\\')
				ignoreNextCharacter = true;

			deduplicatedLoadingPath += character;
		}

		addEntity(deduplicatedLoadingPath);
	}

	m_wolfInstance->evaluateUserInterfaceScript("setSceneName(\"" + sceneName + "\")");
	m_currentSceneName = sceneName;

	m_loadSceneRequest.clear();
}

Entity* SystemManager::addEntity(const std::string& filePath)
{
	Entity* newEntity = new Entity(filePath, [this](Entity*)
		{
			m_entityChangedMutex.lock();
			m_entityChanged = true;
			m_entityChangedMutex.unlock();
		});
	m_entityContainer->addEntity(newEntity);

	const std::string scriptToAddModelToList = "addEntityToList(\"" + newEntity->getName() + "\", \"" + newEntity->computeEscapedLoadingPath() + "\")";
	m_wolfInstance->evaluateUserInterfaceScript(scriptToAddModelToList);

	if (g_editorConfiguration->getEnableRayTracing())
	{
		m_rayTracedWorldBuildNeeded = true;
	}

	return newEntity;
}

void SystemManager::duplicateEntity(const Wolf::ResourceNonOwner<Entity>& entityToDuplicate, const std::string& filePath)
{
	std::filesystem::copy(m_configuration->computeFullPathFromLocalPath(entityToDuplicate->getLoadingPath()), m_configuration->computeFullPathFromLocalPath(filePath));
	Entity* newEntity = addEntity(filePath);
	newEntity->setName(entityToDuplicate->getName() + " - Copy");
}

void SystemManager::addComponent(const std::string& componentId)
{
	if (!m_selectedEntity)
	{
		Wolf::Debug::sendError("No entity selected, can't add component");
		return;
	}
		
	(*m_selectedEntity)->addComponent(m_componentInstancier->instanciateComponent(componentId));
	updateUISelectedEntity();
}

void SystemManager::addFakeEntities()
{
	MaterialListFakeEntity* materialListFakeEntity = new MaterialListFakeEntity(m_wolfInstance->getMaterialsManager(), m_configuration.createNonOwnerResource());
	m_entityContainer->addEntity(materialListFakeEntity);
	m_wolfInstance->evaluateUserInterfaceScript("addEntityToList(\"" + materialListFakeEntity->getName() + "\", \"" + materialListFakeEntity->computeEscapedLoadingPath() + "\", true)");

	GraphicSettingsFakeEntity* graphicSettingsFakeEntity = new GraphicSettingsFakeEntity(m_renderer.createNonOwnerResource<RenderingPipelineInterface>());
	m_entityContainer->addEntity(graphicSettingsFakeEntity);
	m_wolfInstance->evaluateUserInterfaceScript("addEntityToList(\"" + graphicSettingsFakeEntity->getName() + "\", \"" + graphicSettingsFakeEntity->computeEscapedLoadingPath() + "\", true)");
}

void SystemManager::updateUISelectedEntity() const
{
	(*m_selectedEntity)->activateParams();
	std::string jsonParams;
	(*m_selectedEntity)->fillJSONForParams(jsonParams);

	std::string jsFunctionCall = "setNewParams(\"";
	std::string escapedJSON;
	for (const char character : jsonParams)
	{
		if (character == '\\')
			escapedJSON += R"(\\\\)";
		else if (character == '\"')
			escapedJSON += "\\\"";
		else if (character == '\n')
			continue;
		else if (character == '\t')
			escapedJSON += ' ';
		else
			escapedJSON += character;
	}
	jsFunctionCall += escapedJSON;
	jsFunctionCall += "\")";
	m_wolfInstance->evaluateUserInterfaceScript(jsFunctionCall);
	m_wolfInstance->evaluateUserInterfaceScript("refreshWindowSize()");
}
