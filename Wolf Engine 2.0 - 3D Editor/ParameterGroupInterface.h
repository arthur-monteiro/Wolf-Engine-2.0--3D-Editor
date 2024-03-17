#pragma once

#include "EditorTypes.h"

class ParameterGroupInterface
{
public:
	ParameterGroupInterface(const std::string& tab);
	virtual ~ParameterGroupInterface() = default;

	void activateParams();
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount, bool isLast) const;
	virtual std::span<EditorParamInterface*> getAllParams() = 0;
	virtual std::span<EditorParamInterface* const> getAllConstParams() const = 0;
	virtual bool hasDefaultName() const = 0;

	EditorParamInterface* getNameParam() { return &m_name; }

protected:
	std::string m_currentCategory;
	EditorParamString m_name;

private:
	void onNameChanged();
};