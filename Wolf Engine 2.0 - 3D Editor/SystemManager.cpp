#include "SystemManager.h"

#include <ctime>
#include <chrono>
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>

#include <JSONReader.h>

#include "BuildingModel.h"
#include "ObjectModel.h"

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
	jsObject["getFrameRate"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getFrameRate, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["addModel"] = std::bind(&SystemManager::addModelJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["pickFile"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::pickFile, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderHeight"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderHeight, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderWidth"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderWidth, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["getRenderOffsetLeft"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderOffsetLeft, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["setRenderOffsetLeft"] = std::bind(&SystemManager::setRenderOffsetLeft, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["getRenderOffsetRight"] = static_cast<ultralight::JSCallbackWithRetval>(std::bind(&SystemManager::getRenderOffsetRight, this, std::placeholders::_1, std::placeholders::_2));
	jsObject["setRenderOffsetRight"] = std::bind(&SystemManager::setRenderOffsetRight, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeScaleX"] = std::bind(&SystemManager::changeScaleX, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeScaleY"] = std::bind(&SystemManager::changeScaleY, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeScaleZ"] = std::bind(&SystemManager::changeScaleZ, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeRotation"] = std::bind(&SystemManager::changeRotation, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeTranslation"] = std::bind(&SystemManager::changeTranslation, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingSizeX"] = std::bind(&SystemManager::changeBuildingSizeX, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingSizeZ"] = std::bind(&SystemManager::changeBuildingSizeZ, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingFloorCount"] = std::bind(&SystemManager::changeBuildingFloorCount, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingWindowSideSize"] = std::bind(&SystemManager::changeBuildingWindowSideSize, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["changeBuildingWindowMesh"] = std::bind(&SystemManager::changeBuildingWindowMesh, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["selectModelByName"] = std::bind(&SystemManager::selectModelByName, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["saveScene"] = std::bind(&SystemManager::saveScene, this, std::placeholders::_1, std::placeholders::_2);
	jsObject["loadScene"] = std::bind(&SystemManager::loadSceneJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void SystemManager::resizeCallback(uint32_t width, uint32_t height) const
{
	m_editorParams->setWindowWidth(width);
	m_editorParams->setWindowHeight(height);

	m_wolfInstance->evaluateUserInterfaceScript("refreshWindowSize()");
}

ultralight::JSValue SystemManager::getFrameRate(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string fpsStr = "FPS: " + std::to_string(m_stableFPS);
	return { fpsStr.c_str() };
}

enum class BrowseToFileOption { FILE_OPEN, FILE_SAVE };
enum class BrowseToFileFilter { SAVE, OBJ, BUILDING };
#ifdef _WIN32
#include <shtypes.h>
#include <ShlObj_core.h>
#include <codecvt>
#undef ERROR
void BrowseToFile(std::string& filename, BrowseToFileOption option, BrowseToFileFilter filter)
{
	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	char szFile[MAX_PATH];
	ofn.lpstrFile = szFile;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	switch (filter)
	{
		case BrowseToFileFilter::SAVE:
			ofn.lpstrFilter = "Wolf Editor Save (JSON)\0*.json\0";
			break;
		case BrowseToFileFilter::OBJ:
			ofn.lpstrFilter = "Object (OBJ)\0*.obj\0";
			break;
		case BrowseToFileFilter::BUILDING:
			ofn.lpstrFilter = "Building (JSON)\0*.json\0";
			break;
	}
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if ((option == BrowseToFileOption::FILE_SAVE && GetSaveFileNameA(&ofn)) ||
		(option == BrowseToFileOption::FILE_OPEN && GetOpenFileNameA(&ofn)))
	{
		filename = szFile;
	}
}
#endif

ultralight::JSValue SystemManager::pickFile(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const std::string inputOption = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	BrowseToFileOption browseToFileOption;
	if (inputOption == "open")
		browseToFileOption = BrowseToFileOption::FILE_OPEN;
	else if(inputOption == "save")
		browseToFileOption = BrowseToFileOption::FILE_SAVE;
	else
	{
		debugCallback(Debug::Severity::ERROR,  Debug::Type::WOLF, "Unsupported pick file option \"" + inputOption + "\"");
		return "";
	}

	const std::string inputFilter = static_cast<ultralight::String>(args[1].ToString()).utf8().data();
	BrowseToFileFilter browseToFileFilter;
	if (inputFilter == "obj")
		browseToFileFilter = BrowseToFileFilter::OBJ;
	else if (inputFilter == "save")
		browseToFileFilter = BrowseToFileFilter::SAVE;
	else if (inputFilter == "building")
		browseToFileFilter = BrowseToFileFilter::BUILDING;
	else
	{
		debugCallback(Debug::Severity::ERROR, Debug::Type::WOLF, "Unsupported pick file filter \"" + inputFilter + "\"");
		return "";
	}

	std::string filename;
#ifdef _WIN32
	BrowseToFile(filename, browseToFileOption, browseToFileFilter);
#else
	debugCallback(Debug::Severity::ERROR, Debug::Type::WOLF, "Pick file not implemented for this platform");
#endif

	return filename.c_str();
}

ultralight::JSValue SystemManager::getRenderHeight(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderHeight());
	return r.c_str();
}

ultralight::JSValue SystemManager::getRenderWidth(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderWidth());
	return r.c_str();
}

ultralight::JSValue SystemManager::getRenderOffsetRight(const ultralight::JSObject& thisObject,
                                                        const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderOffsetRight());
	return r.c_str();
}

ultralight::JSValue SystemManager::getRenderOffsetLeft(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	const std::string r = std::to_string(m_editorParams->getRenderOffsetLeft());
	return r.c_str();
}

void SystemManager::setRenderOffsetLeft(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const uint32_t value = static_cast<uint32_t>(args[0].ToNumber());
	m_editorParams->setRenderOffsetLeft(value);
}

void SystemManager::setRenderOffsetRight(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const uint32_t value = static_cast<uint32_t>(args[0].ToNumber());
	m_editorParams->setRenderOffsetRight(value);
}

void SystemManager::addModelJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string filepath = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	const std::string materialPath = static_cast<ultralight::String>(args[1].ToString()).utf8().data();
	const std::string type = static_cast<ultralight::String>(args[2].ToString()).utf8().data();

	if (type == "staticMesh")
		addStaticModel(filepath, materialPath, glm::mat4(1.0f));
	else if (type == "building")
		addBuildingModel(filepath, glm::mat4(1.0f));
	else
		debugCallback(Debug::Severity::ERROR, Debug::Type::WOLF, "Model type \"" + type + "\" is not implemented");
}

void SystemManager::changeScaleX(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setScale(0, value);
}

void SystemManager::changeScaleY(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setScale(1, value);
}

void SystemManager::changeScaleZ(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const float value = static_cast<float>(args[0].ToNumber());
	m_selectedModel->setScale(2, value);
}

void SystemManager::changeRotation(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const uint32_t componentIdx = static_cast<uint32_t>(args[0].ToNumber());
	const float value = static_cast<float>(args[1].ToNumber());

	m_selectedModel->setRotation(componentIdx, value);
}

void SystemManager::changeTranslation(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	const uint32_t componentIdx = static_cast<uint32_t>(args[0].ToNumber());
	const float value = static_cast<float>(args[1].ToNumber());

	m_selectedModel->setTranslation(componentIdx, value);
}

void SystemManager::changeBuildingSizeX(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		debugCallback(Debug::Severity::ERROR, Debug::Type::WOLF, "Can't change building size of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);
	
	const float value = static_cast<float>(args[0].ToNumber());
	selectedBuilding->setBuildingSizeX(value);
}

void SystemManager::changeBuildingSizeZ(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args) const
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		debugCallback(Debug::Severity::ERROR, Debug::Type::WOLF, "Can't change building size of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);

	const float value = static_cast<float>(args[0].ToNumber());
	selectedBuilding->setBuildingSizeZ(value);
}

void SystemManager::changeBuildingWindowSideSize(const ultralight::JSObject& thisObject,
                                                 const ultralight::JSArgs& args) const
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		debugCallback(Debug::Severity::ERROR, Debug::Type::WOLF, "Can't change building window size of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);

	const float value = static_cast<float>(args[0].ToNumber());
	selectedBuilding->setWindowSideSize(value);
}

void SystemManager::changeBuildingFloorCount(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args) const
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		debugCallback(Debug::Severity::ERROR, Debug::Type::WOLF, "Can't change building floor count of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);

	const uint32_t value = static_cast<uint32_t>(args[0].ToNumber());
	selectedBuilding->setFloorCount(value);
}

void SystemManager::changeBuildingWindowMesh(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args)
{
	if (m_selectedModel->getType() != ModelInterface::ModelType::BUILDING)
	{
		debugCallback(Debug::Severity::ERROR, Debug::Type::WOLF, "Can't change building window mesh of non building model");
		return;
	}
	BuildingModel* selectedBuilding = static_cast<BuildingModel*>(m_selectedModel);

	const uint32_t meshIdx = static_cast<uint32_t>(args[0].ToNumber());
	const std::string filename = static_cast<ultralight::String>(args[1].ToString()).utf8().data();
	const std::string materialFolder = static_cast<ultralight::String>(args[2].ToString()).utf8().data();
	selectedBuilding->loadWindowMesh(filename, materialFolder, m_currentBindlessOffset / 5);

	std::vector<Image*> modelImages;
	selectedBuilding->getImages(modelImages);
	m_currentBindlessOffset = m_bindlessDescriptor->addImages(modelImages) + modelImages.size();
}

void SystemManager::selectModelByName(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
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

void SystemManager::saveScene(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
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

	debugCallback(Debug::Severity::INFO, Debug::Type::WOLF, "Save successful!");
}

void SystemManager::loadSceneJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string filename = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	m_loadSceneRequest = filename;
}

void SystemManager::updateBeforeFrame()
{
	const uint32_t contextId = m_wolfInstance->getCurrentFrame() % g_configuration->getMaxCachedFrames();

	if(!m_loadSceneRequest.empty())
	{
		loadScene();
	}

	m_modelsContainer->moveToNextFrame();
	const std::vector<std::unique_ptr<ModelInterface>>& allModels = m_modelsContainer->getModels();
	m_gameContexts[contextId].m_modelsToRenderWithDefaultPipeline.clear();
	m_gameContexts[contextId].m_modelsToRenderWithBuildingPipeline.clear();
	for (const std::unique_ptr<ModelInterface>& model : allModels)
	{
		if (model->getType() == ModelInterface::ModelType::BUILDING)
			m_gameContexts[contextId].m_modelsToRenderWithBuildingPipeline.push_back(model.get());
		else
			m_gameContexts[contextId].m_modelsToRenderWithDefaultPipeline.push_back(model.get());
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
	model->getImages(modelImages);
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
