#pragma once

#include <memory>
#include <vector>

#include "EditorModelInterface.h"

class ModelsContainer
{
public:
	void addModel(EditorModelInterface* model);
	void moveToNextFrame();

	void clear();

	const std::vector<std::unique_ptr<EditorModelInterface>>& getModels() { return m_currentModels; }

private:
	std::vector<std::unique_ptr<EditorModelInterface>> m_currentModels;
	std::vector<std::unique_ptr<EditorModelInterface>> m_newModels;

};