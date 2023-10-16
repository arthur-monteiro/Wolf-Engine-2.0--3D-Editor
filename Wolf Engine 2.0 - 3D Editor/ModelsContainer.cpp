#include "ModelsContainer.h"

void ModelsContainer::addModel(ModelInterface* model)
{
	m_newModels.push_back(std::unique_ptr<ModelInterface>(model));
}

void ModelsContainer::moveToNextFrame()
{
	for(std::unique_ptr<ModelInterface>& model : m_newModels)
	{
		m_currentModels.push_back(std::unique_ptr<ModelInterface>(model.release()));
	}
	m_newModels.clear();
}

void ModelsContainer::clear()
{
	m_currentModels.clear();
	m_newModels.clear();
}
