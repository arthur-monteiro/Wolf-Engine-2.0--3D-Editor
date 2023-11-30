#pragma once

class EditorModelInterface;

struct GameContext
{
	enum class DisplayType
	{
		ALBEDO = 0,
		NORMAL = 1,
		ROUGHNESS = 2,
		METALNESS = 3,
		MAT_AO = 4
	};
	DisplayType displayType = DisplayType::ALBEDO;
};

