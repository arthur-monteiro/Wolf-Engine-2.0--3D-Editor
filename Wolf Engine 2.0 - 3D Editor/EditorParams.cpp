#include "EditorParams.h"

float EditorParams::getAspect()
{
	return static_cast<float>(getRenderWidth()) / static_cast<float>(getRenderHeight());
}

VkViewport EditorParams::getRenderViewport()
{
	VkViewport viewport{};
	viewport.height = static_cast<float>(getRenderHeight());
	viewport.width = static_cast<float>(getRenderWidth());
	viewport.x = static_cast<float>(getRenderOffsetLeft());
	viewport.y = static_cast<float>(getRenderOffsetTop());
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	return viewport;
}

uint32_t EditorParams::getRenderOffsetLeft()
{
	return 500;
}

uint32_t EditorParams::getRenderOffsetRight()
{
	return 300;
}

uint32_t EditorParams::getRenderWidth()
{
	return m_width - getRenderOffsetLeft() - getRenderOffsetRight();
}

uint32_t EditorParams::getRenderOffsetTop()
{
	return 100;
}

uint32_t EditorParams::getRenderOffsetBot()
{
	return 50;
}

uint32_t EditorParams::getRenderHeight()
{
	return m_height - getRenderOffsetTop() - getRenderOffsetBot();
}
