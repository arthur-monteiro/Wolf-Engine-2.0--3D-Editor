#pragma once

#include <memory>
#include <vector>

#include "ModelInterface.h"

class ModelsContainer
{
public:
	void addModel(ModelInterface* model);
	void moveToNextFrame();

	void clear();

	const std::vector<std::unique_ptr<ModelInterface>>& getModels() { return m_currentModels; }

private:
	std::vector<std::unique_ptr<ModelInterface>> m_currentModels;
	std::vector<std::unique_ptr<ModelInterface>> m_newModels;

};