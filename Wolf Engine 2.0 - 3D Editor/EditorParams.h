#pragma once

#include <vulkan/vulkan_core.h>

class EditorParams
{
public:
	EditorParams(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}

	void setWindowWidth(uint32_t width) { m_width = width; }
	void setWindowHeight(uint32_t height) { m_height = height; }

	float getAspect();
	VkViewport getRenderViewport();

	uint32_t getRenderOffsetLeft();
	uint32_t getRenderOffsetRight();
	uint32_t getRenderWidth();
	uint32_t getRenderOffsetTop();
	uint32_t getRenderOffsetBot();
	uint32_t getRenderHeight();

private:
	uint32_t m_width;
	uint32_t m_height;
};

