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
		MAT_AO = 4,
		ANISO_STRENGTH = 5,
		LIGHTING = 6,
		ENTITY_IDX = 7,
		RAY_TRACED_WORLD_DEBUG_ALBEDO = 8,
		RAY_TRACED_WORLD_DEBUG_INSTANCE_ID = 9,
		RAY_TRACED_WORLD_DEBUG_PRIMITIVE_ID = 10
	};
	DisplayType displayType = DisplayType::ALBEDO;
};

