#pragma once

#include <string>

class EditorConfiguration
{
public:
	EditorConfiguration(const std::string& filePath);

	[[nodiscard]] const std::string& getDataFolderPath() const { return m_dataFolderPath; }
	[[nodiscard]] const std::string& getDefaultScene() const { return m_defaultScene; }

	std::string computeFullPathFromLocalPath(const std::string& localPath) const { return m_dataFolderPath + '/' + localPath; }
	std::string computeLocalPathFromFullPath(const std::string& fullPath) const { return fullPath.substr(m_dataFolderPath.size() + 1); }

#ifdef __ANDROID__
	AAssetManager* getAndroidAssetManager() const { return m_androidAssetManager; }
#endif

private:
	std::string m_dataFolderPath;
	std::string m_defaultScene;
};

extern const EditorConfiguration* g_editorConfiguration;

