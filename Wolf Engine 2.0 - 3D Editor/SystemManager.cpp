#include "SystemManager.h"

#include <ctime>
#include <chrono>
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>

#include <JSONReader.h>

#include "BuildingModel.h"
#include "ObjectModel.h"

enum class BrowseToFileOption;
using namespace Wolf;

SystemManager::SystemManager()
{
	createWolfInstance();
	m_bindlessDescriptor.reset(new BindlessDescriptor);
	m_editorParams.reset(new EditorParams(g_configuration->getWindowWidth(), g_configuration->getWindowHeight()));

	createMainRenderer();

	m_modelsContainer.reset(new ModelsContainer);
	m_camera.reset(new Camera(glm::vec3(1.4f, 1.2f, 0.3f), glm::vec3(2.0f, 0.9f, -0.3f), glm::vec3(0.0f, 1.0f, 0.0f), 0.01f, 5.0f, 16.0f / 9.0f));
	m_wolfInstance->setCameraInterface(m_camera.get());
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
			m_stableFPS = std::round((1000.0f * m_currentFramesAccumulated) / static_cast<float>(durationInMs));

			m_currentFramesAccumulated = 0;
			m_startTimeFPSCounter = currentTime;
		}
	}

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
}

void SystemManager::createMainRenderer()
{
	m_mainRenderer.reset(new MainRenderingPipeline(m_wolfInstance.get(), m_bindlessDescriptor.get(), m_editorParams.get()));
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
		else if(character == '\"')
			escapedMessage += "\\\"";
		else
			escapedMessage += character;
	}

	if(m_wolfInstance)
	{
		switch (severity)
		{
		case Debug::Severity::ERROR:
			m_wolfInstance->evaluateUserInterfaceScript("addLog(\"" + escapedMessage + "\", \"logError\")");
			break;
		case Debug::Severity::WARNING:
			break;
		case Debug::Severity::INFO:
			m_wolfInstance->evaluateUserInterfaceScript("addLog(\"" + escapedMessage + "\", \"logInfo\")");
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
	jsObject["addModel"] = std::bind(&SystemManager::addModelJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["pickFile"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::pickFileJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderHeight"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderHeightJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderWidth"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderWidthJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderOffsetLeft"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderOffsetLeftJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["setRenderOffsetLeft"] = std::bind(&SystemManager::setRenderOffsetLeftJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["getRenderOffsetRight"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderOffsetRightJSCallback, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["setRenderOffsetRight"] = std::bind(&SystemManager::setRenderOffsetRightJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeScaleX"] = std::bind(&SystemManager::changeScaleXJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeScaleY"] = std::bind(&SystemManager::changeScaleYJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeScaleZ"] = std::bind(&SystemManager::changeScaleZJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeRotationX"] = std::bind(&SystemManager::changeRotationXJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeRotationY"] = std::bind(&SystemManager::changeRotationYJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeRotationZ"] = std::bind(&SystemManager::changeRotationZJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeTranslationX"] = std::bind(&SystemManager::changeTranslationXJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeTranslationY"] = std::bind(&SystemManager::changeTranslationYJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeTranslationZ"] = std::bind(&SystemManager::changeTranslationZJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingSizeX"] = std::bind(&SystemManager::changeBuildingSizeXJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingSizeZ"] = std::bind(&SystemManager::changeBuildingSizeZJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingFloorCount"] = std::bind(&SystemManager::changeBuildingFloorCountJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingWindowSideSize"] = std::bind(&SystemManager::changeBuildingWindowSideSizeJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingWindowMesh"] = std::bind(&SystemManager::changeBuildingWindowMeshJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingWallMesh"] = std::bind(&SystemManager::changeBuildingWallMeshJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["selectModelByName"] = std::bind(&SystemManager::selectModelByNameJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["saveScene"] = std::bind(&SystemManager::saveSceneJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["loadScene"] = std::bind(&SystemManager::loadSceneJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["displayTypeSelectChanged"] = std::bind(&SystemManager::displayTypeSelectChangedJSCallback, this, std::placeholders::_1, std::placeholders::_2);
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
	std::array<char, 128> buffer;
	std::string result;
	FILE* pipe = _popen(cmd, "r");
	if (!pipe) 
		Debug::sendError("Can't open " + std::string(cmd));
	
	while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) 
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

	std::string filename;
#ifdef _WIN32
	const std::string cmd = "BrowseToFile.exe " + inputOption + " " + inputFilter;
	filename = exec(cmd.c_str());
#else
	Debug::sendError("Pick file not implemented for this platform");
#endif

	return filename.c_str();
}

ultralight::JSValue SystemManager::pickFolderJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
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

ultralight::JSValue SystemManager::getRenderWidthJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderWidth());
	return r.c_str();
}

ultralight::JSValue SystemManager::getRenderOffsetRightJSCallback(const ultralight::JSObject& thisObject,
                                                        const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderOffsetRight());
	return r.c_str();
}

ultralight::JSValue SystemManager::getRenderOffsetLeftJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
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

void SystemManager::addModelJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string filepath = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	const std::string materialPath = static_cast<ultralight::String>(args[1].ToString()).utf8().data();
	const std::string type = static_cast<ultralight::String>(args[2].ToString()).utf8().data();

	m_addModelRequests.push_back({ filepath, materialPath, type });
}

void SystemManager::changeScaleXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setScale(0, value);
}

void SystemManager::changeScaleYJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setScale(1, value);
}

void SystemManager::changeScaleZJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setScale(2, value);
}

void SystemManager::changeRotationXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setRotation(0, value);
}

void SystemManager::changeRotationYJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setRotation(1, value);
}

void SystemManager::changeRotationZJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setRotation(2, value);
}

void SystemManager::changeTranslationXJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setTranslation(0, value);
}

void SystemManager::changeTranslationYJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setTranslation(1, value);
}

void SystemManager::changeTranslationZJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setTranslation(2, value);
}

void SystemManager::changeBuildingSizeXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		Debug::sendError("Can't change building size of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);
	
	const float value = static_cast<float>(args[0].ToNumber());
	selectedBuilding->setBuildingSizeX(value);
}

void SystemManager::changeBuildingSizeZJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		Debug::sendError("Can't change building size of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);

	const float value = static_cast<float>(args[0].ToNumber());
	selectedBuilding->setBuildingSizeZ(value);
}

void SystemManager::changeBuildingWindowSideSizeJSCallback(const ultralight::JSObject& thisObject,
                                                 const ultralight::JSArgs& args) const
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		Debug::sendError("Can't change building window size of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);

	const float value = static_cast<float>(args[0].ToNumber());
	selectedBuilding->setWindowSideSize(value);
}

void SystemManager::changeBuildingFloorCountJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		Debug::sendError("Can't change building floor count of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);

	const uint32_t value = static_cast<uint32_t>(args[0].ToNumber());
	selectedBuilding->setFloorCount(value);
}

void SystemManager::changeBuildingWindowMeshJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args)
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		Debug::sendError("Can't change building window mesh of non building model");
		return;
	}

	const uint32_t meshIdx = static_cast<uint32_t>(args[0].ToNumber());
	const std::string filename = static_cast<ultralight::String>(args[1].ToString()).utf8().data();
	const std::string materialFolder = static_cast<ultralight::String>(args[2].ToString()).utf8().data();

	m_changeBuildingPieceMeshRequests.push_back({ meshIdx, filename, materialFolder, BuildingModel::PieceType::WINDOW });
}

void SystemManager::changeBuildingWallMeshJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args)
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		Debug::sendError("Can't change building wall mesh of non building model");
		return;
	}

	const uint32_t meshIdx = static_cast<uint32_t>(args[0].ToNumber());
	const std::string filename = static_cast<ultralight::String>(args[1].ToString()).utf8().data();
	const std::string materialFolder = static_cast<ultralight::String>(args[2].ToString()).utf8().data();

	m_changeBuildingPieceMeshRequests.push_back({ meshIdx, filename, materialFolder, BuildingModel::PieceType::WALL });
}

void SystemManager::selectModelByNameJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string name = static_cast<ultralight::String>(args[0].ToString()).utf8().data();

	const std::vector<std::unique_ptr<ModelInterface>>& allModels = m_modelsContainer->getModels();
	for (const std::unique_ptr<ModelInterface>& model : allModels)
	{
		if(model->getName() == name)
		{
			m_selectedModel = model.get();
			updateUISelectedModel();
			break;
		}
	}
}

void SystemManager::saveSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string& outputFilename = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	const std::string& sceneName = static_cast<ultralight::String>(args[1].ToString()).utf8().data();
	m_currentSceneName = sceneName;
	m_wolfInstance->evaluateUserInterfaceScript("setSceneName(\"" + m_currentSceneName + "\")");

	std::ofstream outputFile(outputFilename);

	// Header comments
	const time_t now = time(nullptr);
	tm newTime;
	localtime_s(&newTime, &now);
	outputFile << "// Save scene: " << newTime.tm_mday << "/" << 1 + newTime.tm_mon << "/" << 1900 + newTime.tm_year << " " << newTime.tm_hour << ":" << newTime.tm_min << "\n\n";

	outputFile << "{\n";

	// Scene data
	const std::vector<std::unique_ptr<ModelInterface>>& allModels = m_modelsContainer->getModels();
	outputFile << "\t\"sceneName\":\"" << m_currentSceneName << "\",\n";
	outputFile << "\t\"modelCount\":" << allModels.size() << ",\n";

	// Model infos
	outputFile << "\t\"models\": [\n";
	for (uint32_t i = 0; i < allModels.size(); ++i)
	{
		const std::unique_ptr<ModelInterface>& model = allModels[i];

		outputFile << "\t\t{\n";
		outputFile << "\t\t\t\"modelName\":\"" << model->getName() << "\",\n";

		const ModelInterface::ModelType modelType = model->getType();
		outputFile << "\t\t\t\"modelType\":\"" << ModelInterface::convertModelTypeToString(modelType) << "\",\n";

		if (modelType == ModelInterface::ModelType::BUILDING)
			static_cast<BuildingModel*>(model.get())->save();

		const std::string& loadingPath = model->getLoadingPath();
		std::string escapedLoadingPath;
		for(const char character : loadingPath)
		{
			if (character == '\\')
				escapedLoadingPath += "\\\\";
			else
				escapedLoadingPath += character;
		}
		outputFile << "\t\t\t\"loadingPath\":\"" << escapedLoadingPath << "\",\n";

		const glm::mat4& transform = model->getTransform();
		outputFile << "\t\t\t\"transform\":[";
		for(uint32_t matI = 0; matI < 4; ++matI)
		{
			for (uint32_t matJ = 0; matJ < 4; ++matJ)
			{
				outputFile << transform[matI][matJ];
				if (matI != 3 || matJ != 3)
					outputFile << ",";
			}

			if (matI != 3)
				outputFile << "\n\t\t\t             ";
		}
		outputFile << "]\n";

		outputFile << "\t\t}";
		if (i != allModels.size() - 1)
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
	const std::string filename = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	m_loadSceneRequest = filename;
}

void SystemManager::displayTypeSelectChangedJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args)
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

void SystemManager::updateBeforeFrame()
{
	const uint32_t contextId = m_wolfInstance->getCurrentFrame() % g_configuration->getMaxCachedFrames();

	if(!m_loadSceneRequest.empty())
	{
		loadScene();
	}

	for (const AddModelRequest& request : m_addModelRequests)
	{
		if (request.type == "staticMesh")
			addStaticModel(request.filepath, request.materialPath, glm::mat4(1.0f));
		else if (request.type == "building")
			addBuildingModel(request.filepath, glm::mat4(1.0f));
		else
			Debug::sendError("Model type \"" + request.type + "\" is not implemented");
	}
	m_addModelRequests.clear();

	for (const ChangeBuildingPieceMeshRequest& request : m_changeBuildingPieceMeshRequests)
	{
		BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);
		selectedBuilding->loadPieceMesh(request.filename, request.materialFolder, m_currentBindlessOffset / 5, request.pieceType);

		std::vector<Image*> modelImages;
		selectedBuilding->getImagesForPiece(modelImages, request.pieceType);
		m_currentBindlessOffset = m_bindlessDescriptor->addImages(modelImages) + modelImages.size();
	}
	m_changeBuildingPieceMeshRequests.clear();

	m_modelsContainer->moveToNextFrame();
	const std::vector<std::unique_ptr<ModelInterface>>& allModels = m_modelsContainer->getModels();
	m_gameContexts[contextId].modelsToRenderWithDefaultPipeline.clear();
	m_gameContexts[contextId].modelsToRenderWithBuildingPipeline.clear();
	for (const std::unique_ptr<ModelInterface>& model : allModels)
	{
		if (model->getType() == ModelInterface::ModelType::BUILDING)
			m_gameContexts[contextId].modelsToRenderWithBuildingPipeline.push_back(model.get());
		else
			m_gameContexts[contextId].modelsToRenderWithDefaultPipeline.push_back(model.get());
		model->updateGraphic(*m_camera);
	}

	m_camera->setAspect(m_editorParams->getAspect());

	m_wolfInstance->updateEvents();
	m_mainRenderer->update(m_wolfInstance.get());

	if (m_wolfInstance->getInputHandler().keyPressedThisFrame(GLFW_KEY_ESCAPE))
	{
		m_isCameraLocked = !m_isCameraLocked;
		m_camera->setLocked(m_isCameraLocked);
	}

	// Select object by click
	if (m_wolfInstance->getInputHandler().mouseButtonPressedThisFrame(GLFW_MOUSE_BUTTON_LEFT))
	{
		const glm::vec3 rayOrigin = m_camera->getPosition();

		float currentMousePosX, currentMousePosY;
		m_wolfInstance->getInputHandler().getMousePosition(currentMousePosX, currentMousePosY);

		const float renderPosX = currentMousePosX - m_editorParams->getRenderOffsetLeft();
		const float renderPosY = currentMousePosY - m_editorParams->getRenderOffsetBot();

		if (renderPosX < 0.0f || renderPosX > m_editorParams->getRenderWidth() || renderPosY < 0.0f || renderPosY > m_editorParams->getRenderHeight())
			return;

		const float renderPosClipSpaceX = (renderPosX / static_cast<float>(m_editorParams->getRenderWidth())) * 2.0f - 1.0f;
		const float renderPosClipSpaceY = (renderPosY / static_cast<float>(m_editorParams->getRenderHeight())) * 2.0f - 1.0f;

		const glm::vec3 clipTarget = glm::vec3(glm::inverse(m_camera->getProjectionMatrix()) * glm::vec4(renderPosClipSpaceX, renderPosClipSpaceY, 1.0f, 1.0f));
		const glm::vec3 rayDirection = glm::vec3(glm::inverse(m_camera->getViewMatrix()) * glm::vec4(clipTarget, 0.0f));

		m_selectedModel = nullptr;
		float minDistance = 2'000.0f;
		for (const std::unique_ptr<ModelInterface>& model : allModels)
		{
			if(float intersectionDistance = model->getAABB().intersect(rayOrigin, rayDirection); intersectionDistance > AABB::NO_INTERSECTION)
			{
				intersectionDistance = intersectionDistance > 0.0f ? intersectionDistance : 1'000.0f;
				if(intersectionDistance < minDistance)
				{
					minDistance = intersectionDistance;
					m_selectedModel = model.get();
				}
			}
		}

		if(m_selectedModel)
			updateUISelectedModel();
		else
			m_wolfInstance->evaluateUserInterfaceScript("resetSelectedModel()");
	}

	glm::vec3 cameraPosition = m_camera->getPosition();
	m_wolfInstance->evaluateUserInterfaceScript("setCameraPosition(\"" + std::to_string(cameraPosition.x) + ", " + std::to_string(cameraPosition.y) + ", " + std::to_string(cameraPosition.z) + "\")");
}

void SystemManager::loadScene()
{
	m_modelsContainer->clear();
	m_wolfInstance->evaluateUserInterfaceScript("resetModelList()");
	m_wolfInstance->evaluateUserInterfaceScript("resetSelectedModel()");
	m_selectedModel = nullptr;

	JSONReader jsonReader(m_loadSceneRequest);

	const uint32_t nbObject = static_cast<uint32_t>(jsonReader.getRoot()->getPropertyFloat("modelCount"));
	const std::string& sceneName = jsonReader.getRoot()->getPropertyString("sceneName");
	for(uint32_t objectIdx = 0; objectIdx < nbObject; objectIdx++)
	{
		JSONReader::JSONObjectInterface* modelObject = jsonReader.getRoot()->getArrayObjectItem("models", objectIdx);

		const std::string& modelLoadingPath = modelObject->getPropertyString("loadingPath");
		std::string deduplicatedLoadingPath;
		bool ignoreNextCharacter = false;
		for (const char character : modelLoadingPath)
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
		const std::string& modelName = modelObject->getPropertyString("modelName");

		glm::mat4 transform;
		std::memcpy(&transform, modelObject->getPropertyFloatArray("transform").data(), sizeof(glm::mat4));

		const std::string& modelType = modelObject->getPropertyString("modelType");
		if(modelType == "staticMesh")
			addStaticModel(deduplicatedLoadingPath, "", transform);
		else if(modelType == "building")
			addBuildingModel(deduplicatedLoadingPath, transform);
	}
	m_wolfInstance->evaluateUserInterfaceScript("setSceneName(\"" + sceneName + "\")");
	m_currentSceneName = sceneName;

	m_loadSceneRequest.clear();
}

void SystemManager::addStaticModel(const std::string& filepath, const std::string& materialFolder, const glm::mat4& transform)
{
	ObjectModel* model = new ObjectModel(transform, false, filepath, materialFolder, true, m_currentBindlessOffset / 5);
	m_modelsContainer->addModel(model);
	std::vector<Image*> modelImages;
	model->getImages(modelImages);
	m_currentBindlessOffset = m_bindlessDescriptor->addImages(modelImages) + modelImages.size();

	const std::string scriptToAddModelToList = "addModelToList(\"" + model->getName() + "\")";
	m_wolfInstance->evaluateUserInterfaceScript(scriptToAddModelToList);
}

void SystemManager::addBuildingModel(const std::string& filepath, const glm::mat4& transform)
{
	BuildingModel* model = new BuildingModel(transform, filepath, m_currentBindlessOffset / 5);
	m_modelsContainer->addModel(model);
	std::vector<Image*> modelImages;
	model->getAllImages(modelImages);
	m_currentBindlessOffset = m_bindlessDescriptor->addImages(modelImages) + modelImages.size();

	const std::string scriptToAddModelToList = "addModelToList(\"" + model->getName() + "\")";
	m_wolfInstance->evaluateUserInterfaceScript(scriptToAddModelToList);
}

void SystemManager::updateUISelectedModel()
{
	std::string jsFunctionCall = "updateSelectedModelInfo(";
	jsFunctionCall += "\"" + m_selectedModel->getName() + "\", ";

	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 translation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(m_selectedModel->getTransform(), scale, rotation, translation, skew, perspective);
	glm::vec3 eulerRotation = glm::eulerAngles(rotation) * 3.14159f / 180.f;

	auto addVectorComponentsToParameters = [&](const glm::vec3& vector, bool isLast = false)
	{
		jsFunctionCall += std::to_string(vector.x) + ", " + std::to_string(vector.y) + ", " + std::to_string(vector.y) + (isLast ? ")" : ", ");
	};
	addVectorComponentsToParameters(scale);
	addVectorComponentsToParameters(translation);
	addVectorComponentsToParameters(eulerRotation, true);

	m_wolfInstance->evaluateUserInterfaceScript(jsFunctionCall);

	if (m_selectedModel->getType() == ModelInterface::ModelType::BUILDING)
	{
		const BuildingModel* selectedBuilding = static_cast<const BuildingModel*>(m_selectedModel);

		std::string escapedMessage;
		for (const char character : selectedBuilding->getWindowMeshLoadingPath(0))
		{
			if (character == '\\')
				escapedMessage += "\\\\";
			else if (character == '\"')
				escapedMessage += "\\\"";
			else
				escapedMessage += character;
		}

		m_wolfInstance->evaluateUserInterfaceScript("updateSelectedBuildingInfo(" + std::to_string(selectedBuilding->getBuildingSizeX()) + "," + std::to_string(selectedBuilding->getBuildingSizeZ()) + "," +
			std::to_string(selectedBuilding->getFloorCount()) + ", " + std::to_string(selectedBuilding->getWindowSizeSize()) + ", \"" + escapedMessage + "\")");
	}
}
