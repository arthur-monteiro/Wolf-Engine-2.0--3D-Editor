#pragma once

#include "EditorTypes.h"

class ParameterGroupInterface
{
public:
	ParameterGroupInterface(const std::string& tab);
	virtual ~ParameterGroupInterface() = default;

	void activateParams();
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const;
	virtual void getAllParams(std::vector<EditorParamInterface*>& out) const = 0;
	virtual void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const = 0;
	virtual bool hasDefaultName() const = 0;

	EditorParamInterface* getNameParam() { return &m_name; }
	void setName(const std::string& name) { m_name = name; }

protected:
	std::string m_currentCategory;
	EditorParamString m_name;

private:
	void onNameChanged();
};