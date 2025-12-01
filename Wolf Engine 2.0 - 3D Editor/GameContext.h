#pragma once

#include <string>

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
		RAY_TRACED_WORLD_DEBUG_PRIMITIVE_ID = 10,
		PATH_TRACING = 11,
		GLOBAL_IRRADIANCE = 12,
		VERTEX_COLOR = 13
	};
	DisplayType displayType = DisplayType::ALBEDO;

	enum class ShadowTechnique
	{
		CSM,
		RAY_TRACED
	};
	ShadowTechnique shadowTechnique = ShadowTechnique::CSM;

	std::string currentSceneName;
};

static constexpr float CLEAR_VALUE = 0.01002282557f; // pow((0.1 + 0.055) / 1.055, 2.4)

