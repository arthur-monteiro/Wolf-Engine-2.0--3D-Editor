#include "ParameterGroupInterface.h"

#include "EditorParamsHelper.h"

ParameterGroupInterface::ParameterGroupInterface(const std::string& tab) : m_name("Name", tab, "Placeholder category", [this]() { onNameChanged(); }, 
																				  EditorParamString::ParamStringType::STRING, true)
{
	m_currentCategory = m_name;
}

void ParameterGroupInterface::activateParams()
{
	m_name.activate();

	std::vector<EditorParamInterface*> allVisibleParam;
	getAllVisibleParams(allVisibleParam);
	for (EditorParamInterface* param : allVisibleParam)
	{
		param->activate();
	}
}

void ParameterGroupInterface::addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const
{
	m_name.addToJSON(outJSON, tabCount, false);

	std::vector<EditorParamInterface*> allParams;
	getAllVisibleParams(allParams);

	::addParamsToJSON(outJSON, allParams, isLast, tabCount);
}

void ParameterGroupInterface::loadParams(Wolf::JSONReader& jsonReader, const std::string& objectId)
{
	std::vector<EditorParamInterface*> arrayItemParams;
	getAllParams(arrayItemParams);
	arrayItemParams.push_back(getNameParam());
	::loadParams(jsonReader, objectId, arrayItemParams);
}

void ParameterGroupInterface::onNameChanged()
{
	m_name.setCategory(m_name);

	std::vector<EditorParamInterface*> allVisibleParam;
	getAllParams(allVisibleParam);
	for (EditorParamInterface* param : allVisibleParam)
	{
		param->setCategory(m_name);
	}
	m_currentCategory = m_name;
}
