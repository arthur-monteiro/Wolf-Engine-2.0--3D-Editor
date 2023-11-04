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

uint32_t EditorParams::getRenderOffsetLeft() const
{
	return m_renderOffsetLeft;
}

uint32_t EditorParams::getRenderOffsetRight() const
{
	return m_renderOffsetRight;
}

uint32_t EditorParams::getRenderWidth() const
{
	return m_width - getRenderOffsetLeft() - getRenderOffsetRight();
}

uint32_t EditorParams::getRenderOffsetTop() const
{
	return 60;
}

uint32_t EditorParams::getRenderOffsetBot() const
{
	return 50;
}

uint32_t EditorParams::getRenderHeight() const
{
	return m_height - getRenderOffsetTop() - getRenderOffsetBot();
}
