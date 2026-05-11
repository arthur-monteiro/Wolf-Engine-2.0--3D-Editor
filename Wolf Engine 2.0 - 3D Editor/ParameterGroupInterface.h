#pragma once

#include "EditorTypes.h"

class ParameterGroupInterface
{
public:
	ParameterGroupInterface(const std::string& tab, const std::string& category);
	virtual ~ParameterGroupInterface() = default;

	void activateParams(uint32_t arrayIdx = 0);
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const;
	virtual void getAllParams(std::vector<EditorParamInterface*>& out) const = 0;
	virtual void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const = 0;
	virtual bool hasDefaultName() const = 0;

	EditorParamString* getNameParam() { return &m_name; }
	std::string getName() const { return m_name; }
	void setName(const std::string& name) { m_name = name; }

	virtual void loadParams(Wolf::JSONReader::JSONObjectInterface* root, const std::string& objectId);

protected:
	EditorParamString m_name;

	virtual void onNameChanged() { };

private:
	void onNameChangedInternal();
};