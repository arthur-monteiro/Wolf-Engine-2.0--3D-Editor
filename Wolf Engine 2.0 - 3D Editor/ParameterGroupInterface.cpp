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
	for (EditorParamInterface* param : getAllParams())
	{
		param->activate();
	}
}

void ParameterGroupInterface::addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const
{
	m_name.addToJSON(outJSON, tabCount, false);
	::addParamsToJSON(outJSON, getAllConstParams(), isLast, tabCount);
}

void ParameterGroupInterface::onNameChanged()
{
	m_name.setCategory(m_name);
	for (EditorParamInterface* param : getAllParams())
	{
		param->setCategory(m_name);
	}
	m_currentCategory = m_name;
}