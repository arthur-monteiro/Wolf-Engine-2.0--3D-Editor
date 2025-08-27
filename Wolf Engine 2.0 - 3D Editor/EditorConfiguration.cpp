#include "EditorConfiguration.h"

#include <fstream>
#include <ranges>

#include "Debug.h"

const EditorConfiguration* g_editorConfiguration = nullptr;

EditorConfiguration::EditorConfiguration(const std::string& filePath)
{
	if (g_editorConfiguration != nullptr)
		Wolf::Debug::sendError("Editor configuration should not be instanced twice");
	g_editorConfiguration = this;

	std::ifstream configFile(filePath);
	std::string line;
	while (std::getline(configFile, line))
	{
		if (const size_t pos = line.find('='); pos != std::string::npos)
		{
			std::string token = line.substr(0, pos);
			line.erase(0, pos + 1);

			if (token == "dataFolder")
				m_dataFolderPath = line;
			else if (token == "defaultScene")
				m_defaultScene = line;
			else if (token == "enableDebugDraw")
				m_enableDebugDraw = std::stoi(line);
			else if (token == "enableRayTracing")
				m_enableRayTracing = std::stoi(line);
			else if (token == "takeScreenshotAfterFrameCount")
				m_takeScreenshotAfterFrameCount = std::stoi(line);
		}
	}

	configFile.close();
}
