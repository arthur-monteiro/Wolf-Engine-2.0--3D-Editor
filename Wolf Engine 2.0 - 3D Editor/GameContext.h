#pragma once

#include <vector>

class ModelInterface;

struct GameContext
{
	std::vector<ModelInterface*> m_modelsToRenderWithDefaultPipeline;
	std::vector<ModelInterface*> m_modelsToRenderWithBuildingPipeline;
};

