#include "ParameterGroupInterface.h"

#include "EditorParamsHelper.h"

ParameterGroupInterface::ParameterGroupInterface(const std::string& tab, const std::string& category)
: m_name("Name", tab, category, [this]() { onNameChangedInternal(); }, EditorParamString::ParamStringType::STRING)
{
}

void ParameterGroupInterface::activateParams(uint32_t arrayIdx)
{
	m_name.setArrayIndex(arrayIdx);
	m_name.activate();

	std::vector<EditorParamInterface*> allVisibleParam;
	getAllVisibleParams(allVisibleParam);
	for (EditorParamInterface* param : allVisibleParam)
	{
		param->setArrayIndex(arrayIdx);
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

void ParameterGroupInterface::loadParams(Wolf::JSONReader::JSONObjectInterface* root, const std::string& objectId)
{
	std::vector<EditorParamInterface*> arrayItemParams;
	getAllParams(arrayItemParams);
	arrayItemParams.push_back(getNameParam());
	::loadParams(root, objectId, arrayItemParams);
}

void ParameterGroupInterface::onNameChangedInternal()
{
	onNameChanged();
}
