#include "ContaminationReceiver.h"

#include "EditorParamsHelper.h"

void ContaminationReceiver::loadParams(Wolf::JSONReader& jsonReader)
{
}

void ContaminationReceiver::activateParams()
{
	m_dummy.activate();
}

void ContaminationReceiver::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	std::vector<EditorParamInterface*> params = { &m_dummy };
	::addParamsToJSON(outJSON, params, false, tabCount);
}
