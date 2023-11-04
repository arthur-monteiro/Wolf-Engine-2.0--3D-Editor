#pragma once

#include <vector>

class ModelInterface;

struct GameContext
{
	std::vector<ModelInterface*> modelsToRenderWithDefaultPipeline;
	std::vector<ModelInterface*> modelsToRenderWithBuildingPipeline;

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

