#pragma once

#include "ComponentInterface.h"
#include "EditorTypes.h"

class ContaminationReceiver : public ComponentInterface
{
public:
	static inline std::string ID = "contaminationReceiver";
	std::string getId() const override { return ID; }

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

private:
	inline static const std::string TAB = "Contamination receiver";
	EditorParamString m_dummy = EditorParamString("Dummy", TAB, "General");
};