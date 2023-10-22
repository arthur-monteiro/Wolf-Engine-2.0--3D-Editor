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

	void setRenderOffsetLeft(uint32_t value) { m_renderOffsetLeft = value; }
	void setRenderOffsetRight(uint32_t value) { m_renderOffsetRight = value; }

private:
	uint32_t m_width;
	uint32_t m_height;

	uint32_t m_renderOffsetLeft = 500;
	uint32_t m_renderOffsetRight = 300;
};

