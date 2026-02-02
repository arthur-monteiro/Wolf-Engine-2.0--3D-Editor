#pragma once

#include <cstdint>
#include <string>

class EditorConfiguration
{
public:
	EditorConfiguration(const std::string& filePath);

	[[nodiscard]] const std::string& getDataFolderPath() const { return m_dataFolderPath; }
	[[nodiscard]] const std::string& getDefaultScene() const { return m_defaultScene; }
	[[nodiscard]] bool getEnableDebugDraw() const { return m_enableDebugDraw; }
	[[nodiscard]] bool getEnableRayTracing() const { return m_enableRayTracing; }
	[[nodiscard]] uint32_t getTakeScreenshotAfterFrameCount() const { return m_takeScreenshotAfterFrameCount; }
	[[nodiscard]] bool getDisplayLogsToUI() const { return m_displayLogsToUI; }
	[[nodiscard]] bool getDisableThumbnailGeneration() const { return m_disableThumbnailGeneration; }

	void disableRayTracing() { m_enableRayTracing = false;}

	[[nodiscard]] static std::string sanitizeFilePath(const std::string& filePath);
	[[nodiscard]] std::string computeFullPathFromLocalPath(const std::string& localPath) const { return m_dataFolderPath + '/' + localPath; }
	[[nodiscard]] std::string computeLocalPathFromFullPath(const std::string& fullPath) const { return fullPath.substr(m_dataFolderPath.size() + 1); }

#ifdef __ANDROID__
	AAssetManager* getAndroidAssetManager() const { return m_androidAssetManager; }
#endif

private:
	std::string m_dataFolderPath;
	std::string m_defaultScene;
	bool m_enableDebugDraw = false;
	bool m_enableRayTracing = false;
	uint32_t m_takeScreenshotAfterFrameCount = 0;
	bool m_displayLogsToUI = true;
	bool m_disableThumbnailGeneration = false;
};

extern const EditorConfiguration* g_editorConfiguration;

