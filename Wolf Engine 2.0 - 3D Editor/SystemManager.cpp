#include "SystemManager.h"

#include <ctime>
#include <chrono>
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>

#include <JSONReader.h>

enum class BrowseToFileOption;
using namespace Wolf;

SystemManager::SystemManager()
{
	createWolfInstance();
	m_editorParams.reset(new EditorParams(g_configuration->getWindowWidth(), g_configuration->getWindowHeight()));
	m_configuration.reset(new EditorConfiguration("config/editor.ini"));

	createMainRenderer();
	
	m_entityContainer.reset(new EntityContainer);
	m_componentInstancier.reset(new ComponentInstancier(m_wolfInstance->getMaterialsManager(), [this](ComponentInterface*)
		{
			m_entityReloadRequested = true;
		}, [this](const std::string& entityLoadingPath)
		{
			std::vector<ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
			for (ResourceUniqueOwner<Entity>& entity : allEntities)
			{
				if (entity->getLoadingPath() == entityLoadingPath)
					return entity.createNonOwnerResource();
			}
			Debug::sendCriticalError("Entity not found");
			return allEntities[0].createNonOwnerResource();
		},
		m_configuration.createNonOwnerResource()));
	m_camera.reset(new FirstPersonCamera(glm::vec3(1.4f, 1.2f, 0.3f), glm::vec3(2.0f, 0.9f, -0.3f), glm::vec3(0.0f, 1.0f, 0.0f), 0.01f, 5.0f, 16.0f / 9.0f));

	if (!m_configuration->getDefaultScene().empty())
	{
		m_loadSceneRequest = m_configuration->getDefaultScene();
	}

	m_debugRenderingManager.reset(new DebugRenderingManager);
}

void SystemManager::run()
{
	while (!m_wolfInstance->windowShouldClose() /* check if the window should close (for example if the user pressed alt+f4)*/)
	{
		updateBeforeFrame();
		m_mainRenderer->frame(m_wolfInstance.get());

		/* Update FPS counter */
		const auto currentTime = std::chrono::steady_clock::now();
		m_currentFramesAccumulated++;
		const long long durationInMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_startTimeFPSCounter).count();
		if (durationInMs > 1000)
		{
			m_stableFPS = static_cast<uint32_t>(std::round((1000.0f * m_currentFramesAccumulated) / static_cast<float>(durationInMs)));

			m_currentFramesAccumulated = 0;
			m_startTimeFPSCounter = currentTime;
		}
	}

	m_wolfInstance->getRenderMeshList().clear();
	m_wolfInstance->waitIdle();
}

void SystemManager::createWolfInstance()
{
	WolfInstanceCreateInfo wolfInstanceCreateInfo;
	wolfInstanceCreateInfo.configFilename = "config/config.ini";
	wolfInstanceCreateInfo.debugCallback = std::bind(&SystemManager::debugCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	wolfInstanceCreateInfo.resizeCallback = [this](uint32_t width, uint32_t height)
	{
		resizeCallback(width, height);
	};
	wolfInstanceCreateInfo.htmlURL = "UI/UI.html";
	wolfInstanceCreateInfo.bindUltralightCallbacks = [this] { bindUltralightCallbacks(); };
	wolfInstanceCreateInfo.useBindlessDescriptor = true;

	m_wolfInstance.reset(new WolfEngine(wolfInstanceCreateInfo));
	bindUltralightCallbacks();

	m_gameContexts.reserve(g_configuration->getMaxCachedFrames());
	std::vector<void*> contextPtrs(g_configuration->getMaxCachedFrames());
	for (uint32_t i = 0; i < g_configuration->getMaxCachedFrames(); ++i)
	{
		m_gameContexts.emplace_back();
		contextPtrs[i] = &m_gameContexts.back();
	}
	m_wolfInstance->setGameContexts(contextPtrs);

	EditorParamInterface::setGlobalWolfInstance(m_wolfInstance.get());
}

void SystemManager::createMainRenderer()
{
	m_mainRenderer.reset(new MainRenderingPipeline(m_wolfInstance.get(), m_editorParams.get()));
}

void SystemManager::debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message) const
{
	switch (severity)
	{
	case Debug::Severity::ERROR:
		std::cout << "Error : ";
		break;
	case Debug::Severity::WARNING:
		std::cout << "Warning : ";
		break;
	case Debug::Severity::INFO:
		std::cout << "Info : ";
		break;
	case Debug::Severity::VERBOSE:
		return;
	}

	std::cout << message << std::endl;

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

	if(m_wolfInstance)
	{
		switch (severity)
		{
		case Debug::Severity::ERROR:
			m_wolfInstance->evaluateUserInterfaceScript("addLog(\"" + escapedMessage + R"(", "logError"))");
			break;
		case Debug::Severity::WARNING:
			break;
		case Debug::Severity::INFO:
			m_wolfInstance->evaluateUserInterfaceScript("addLog(\"" + escapedMessage + R"(", "logInfo"))");
			break;
		case Debug::Severity::VERBOSE:
			return;
		}
	}
}

void SystemManager::bindUltralightCallbacks()
{
	ultralight::JSObject jsObject;
	m_wolfInstance->getUserInterfaceJSObject(jsObject);
	jsObject["getFrameRate"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getFrameRateJSCallback, this, std::placeholders::_1, std::placeholders::_2));
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
	jsObject["enableEntityPicking"] = std::bind(&SystemManager::enableEntityPickingJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["disableEntityPicking"] = std::bind(&SystemManager::disableEntityPickingJSCallback, this, std::placeholders::_1, std::placeholders::_2);
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

std::string exec(const char* cmd)
{
	std::array<char, 128> buffer{};
	std::string result;
	FILE* pipe = _popen(cmd, "r");
	if (!pipe) 
		Debug::sendError("Can't open " + std::string(cmd));
	
	while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) 
	{
		result += buffer.data();
	}

	if(_pclose(pipe) < 0)
	{
		Debug::sendError("Error with command " + std::string(cmd) + ": " + result);
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

	std::vector<ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
	for (ResourceUniqueOwner<Entity>& entity : allEntities)
	{
		if(entity->getName() == name)
		{
			m_selectedEntity.reset(new ResourceNonOwner<Entity>(entity.createNonOwnerResource()));
			updateUISelectedEntity();
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
	const std::vector<ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
	outputFile << "\t\"sceneName\":\"" << m_currentSceneName << "\",\n";
	outputFile << "\t\"entityCount\":" << allEntities.size() << ",\n";

	// Model infos
	outputFile << "\t\"entities\": [\n";
	for (uint32_t i = 0; i < allEntities.size(); ++i)
	{
		const ResourceUniqueOwner<Entity>& entity = allEntities[i];
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

	Debug::sendInfo("Save successful!");
}

void SystemManager::loadSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string filePath = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	m_loadSceneRequest = filePath;
}

void SystemManager::displayTypeSelectChangedJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const uint32_t contextId = m_wolfInstance->getCurrentFrame() % g_configuration->getMaxCachedFrames();
	const std::string displayType = static_cast<ultralight::String>(args[0].ToString()).utf8().data();


	if (displayType == "albedo")
		m_gameContexts[contextId].displayType = GameContext::DisplayType::ALBEDO;
	else if (displayType == "normal")
		m_gameContexts[contextId].displayType = GameContext::DisplayType::NORMAL;
	else if (displayType == "roughness")
		m_gameContexts[contextId].displayType = GameContext::DisplayType::ROUGHNESS;
	else if (displayType == "metalness")
		m_gameContexts[contextId].displayType = GameContext::DisplayType::METALNESS;
	else if (displayType == "matAO")
		m_gameContexts[contextId].displayType = GameContext::DisplayType::MAT_AO;
	else
		Debug::sendError("Unsupported display type: " + displayType);
}

void SystemManager::openUIInBrowserJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	if(!g_configuration->getUICommands())
		Debug::sendWarning("UI Commands are not saved");

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
				Debug::sendError("Body begin declaration is not written on a dedicated line, this behaviour is not supported");

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

	std::filesystem::copy("UI/", "tmp/UI/");
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

void SystemManager::enableEntityPickingJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args)
{
	m_entityPickingEnabled = true;
}

void SystemManager::disableEntityPickingJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args)
{
	m_entityPickingEnabled = false;
}

void SystemManager::updateBeforeFrame()
{
	if(!m_loadSceneRequest.empty())
	{
		loadScene();
	}

	m_debugRenderingManager->clearBeforeFrame();

	m_entityContainer->moveToNextFrame([this](const std::string& componentId)
		{
			return m_componentInstancier->instanciateComponent(componentId);
		});

	m_entityChangedMutex.lock();
	if (m_entityChanged)
	{
		m_wolfInstance->evaluateUserInterfaceScript("resetEntityList()");
		const std::vector<ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();
		for (const ResourceUniqueOwner<Entity>& entity : allEntities)
		{
			m_wolfInstance->evaluateUserInterfaceScript("addEntityToList(\"" + entity->getName() + "\", \"" + entity->computeEscapedLoadingPath() + "\")");
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
		m_debugRenderingManager->addAABB((*m_selectedEntity)->getAABB());

	std::vector<ResourceUniqueOwner<Entity>>& allEntities = m_entityContainer->getEntities();

	RenderMeshList& renderList = m_wolfInstance->getRenderMeshList();

	for (const ResourceUniqueOwner<Entity>& entity : allEntities)
	{
		entity->updateBeforeFrame();
		entity->addMeshesToRenderList(renderList);
		entity->addDebugInfo(*m_debugRenderingManager);
	}
	m_debugRenderingManager->addMeshesToRenderList(renderList);

	m_wolfInstance->getCameraList().addCameraForThisFrame(m_camera.get(), 0);

	m_camera->setAspect(m_editorParams->getAspect());

	m_wolfInstance->updateBeforeFrame();
	m_mainRenderer->update(m_wolfInstance.get());

	if (m_wolfInstance->getInputHandler()->keyPressedThisFrame(GLFW_KEY_ESCAPE))
	{
		m_isCameraLocked = !m_isCameraLocked;
		m_camera->setLocked(m_isCameraLocked);
	}

	// Select object by click
	if (m_entityPickingEnabled && m_wolfInstance->getInputHandler()->mouseButtonPressedThisFrame(GLFW_MOUSE_BUTTON_LEFT))
	{
		const glm::vec3 rayOrigin = m_camera->getPosition();

		float currentMousePosX, currentMousePosY;
		m_wolfInstance->getInputHandler()->getMousePosition(currentMousePosX, currentMousePosY);

		const float renderPosX = currentMousePosX - static_cast<float>(m_editorParams->getRenderOffsetLeft());
		const float renderPosY = currentMousePosY - static_cast<float>(m_editorParams->getRenderOffsetBot());

		if (renderPosX >= 0.0f && renderPosX < static_cast<float>(m_editorParams->getRenderWidth()) && renderPosY >= 0.0f && renderPosY < static_cast<float>(m_editorParams->getRenderHeight()))
		{
			const float renderPosClipSpaceX = (renderPosX / static_cast<float>(m_editorParams->getRenderWidth())) * 2.0f - 1.0f;
			const float renderPosClipSpaceY = (renderPosY / static_cast<float>(m_editorParams->getRenderHeight())) * 2.0f - 1.0f;

			const glm::vec3 clipTarget = glm::vec3(glm::inverse(m_camera->getProjectionMatrix()) * glm::vec4(renderPosClipSpaceX, renderPosClipSpaceY, 1.0f, 1.0f));
			const glm::vec3 rayDirection = glm::vec3(glm::inverse(m_camera->getViewMatrix()) * glm::vec4(clipTarget, 0.0f));

			float minDistance = 2'000.0f;
			for (ResourceUniqueOwner<Entity>& entity : allEntities)
			{
				if (entity->hasModelComponent())
				{
					if (float intersectionDistance = entity->getAABB().intersect(rayOrigin, rayDirection); intersectionDistance > AABB::NO_INTERSECTION)
					{
						intersectionDistance = intersectionDistance > 0.0f ? intersectionDistance : 1'000.0f;
						if (intersectionDistance < minDistance)
						{
							minDistance = intersectionDistance;
							m_selectedEntity.reset(new ResourceNonOwner<Entity>(entity.createNonOwnerResource()));
						}
					}
				}
			}

			if (m_selectedEntity)
				updateUISelectedEntity();
		}
	}

	const glm::vec3 cameraPosition = m_camera->getPosition();
	m_wolfInstance->evaluateUserInterfaceScript("setCameraPosition(\"" + std::to_string(cameraPosition.x) + ", " + std::to_string(cameraPosition.y) + ", " + std::to_string(cameraPosition.z) + "\")");
}

void SystemManager::loadScene()
{
	m_selectedEntity.reset(nullptr);
	m_entityContainer->clear();

	m_wolfInstance->evaluateUserInterfaceScript("resetEntityList()");
	m_wolfInstance->evaluateUserInterfaceScript("resetSelectedEntity()");

	JSONReader jsonReader(m_configuration->computeFullPathFromLocalPath(m_loadSceneRequest));

	const uint32_t entityCount = static_cast<uint32_t>(jsonReader.getRoot()->getPropertyFloat("entityCount"));
	const std::string& sceneName = jsonReader.getRoot()->getPropertyString("sceneName");
	for(uint32_t entityIdx = 0; entityIdx < entityCount; entityIdx++)
	{
		JSONReader::JSONObjectInterface* entityObject = jsonReader.getRoot()->getArrayObjectItem("entities", entityIdx);

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

void SystemManager::addEntity(const std::string& filePath)
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
}

void SystemManager::addComponent(const std::string& componentId)
{
	if (!m_selectedEntity)
	{
		Debug::sendError("No entity selected, can't add component");
		return;
	}
		
	(*m_selectedEntity)->addComponent(m_componentInstancier->instanciateComponent(componentId));
	updateUISelectedEntity();
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