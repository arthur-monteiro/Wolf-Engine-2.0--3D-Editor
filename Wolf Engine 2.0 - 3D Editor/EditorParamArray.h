#pragma once

#include "EditorTypes.h"
#include "ParameterGroupInterface.h"

template <typename T>
class EditorParamArray : public EditorParamInterface
{
public:
	EditorParamArray(const std::string& name, const std::string& tab, const std::string& category, uint32_t maxSize) : EditorParamInterface(Type::Array, name, tab, category)
	{
		m_maxSize = maxSize;
		m_value.reserve(maxSize);
	}
	EditorParamArray(const std::string& name, const std::string& tab, const std::string& category, uint32_t maxSize, const std::function<void()>& callbackValueChanged) : EditorParamArray(name, tab, category, maxSize)
	{
		m_callbackValueChanged = callbackValueChanged;
	}
	EditorParamArray(const EditorParamArray<T>& other) : EditorParamInterface(Type::Array, other.m_name, other.m_tab, other.m_category)
	{
		m_callbackValueChanged = other.m_callbackValueChanged;
		m_value = other.m_value;
		m_maxSize = other.m_maxSize;
	}

	void activate() override
	{		
		ultralight::JSObject jsObject;
		ms_wolfInstance->getUserInterfaceJSObject(jsObject);

		const std::string functionChangeName = "addTo" + removeSpaces(m_tab) + removeSpaces(m_name) + removeSpaces(m_category);
		jsObject[functionChangeName.c_str()] = std::bind(&EditorParamArray<T>::addValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);

		for (T& value : m_value)
		{
			ParameterGroupInterface* valueAsParameterGroup = static_cast<ParameterGroupInterface*>(&value);
			valueAsParameterGroup->activateParams();
		}
		
	}
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override
	{
		std::string tabs;
		for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

		out += tabs + +"{\n";
		addCommonInfoToJSON(out, tabCount + 1);
		out += tabs + '\t' + R"("count" : )" + std::to_string(m_value.size()) + "\n";
		out += tabs + "}" + (isLast && m_value.empty() ? "\n" : ",\n");

		for (uint32_t i = 0; i < m_value.size(); ++i)
		{
			const ParameterGroupInterface* valueAsParameterGroup = static_cast<const ParameterGroupInterface*>(&m_value[i]);
			valueAsParameterGroup->addParamsToJSON(out, tabCount, isLast && i == m_value.size() - 1);
		}
	}

	size_t size() const { return m_value.size(); }
	bool empty() const { return m_value.empty(); }
	T& operator[](size_t idx) { return m_value[idx]; }
	typename std::vector<T>::iterator begin() { return m_value.begin(); }
	typename std::vector<T>::iterator end() { return m_value.end(); }
	T& back() { return m_value.back(); }
	T& emplace_back() { addValueNoCheck(); return back(); }

private:
	uint32_t m_maxSize;
	std::vector<T> m_value;

	void addValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
	{
		if (!m_value.empty() && m_value.back().hasDefaultName())
		{
			Wolf::Debug::sendError("Can't add a new item as the previous one hasn't been renamed");
			return;
		}

		if (m_value.size() == m_maxSize)
		{
			Wolf::Debug::sendError("Can't add a new item as it's already at max size");
			return;
		}

		addValueNoCheck();
	}
	void addValueNoCheck()
	{
		m_value.resize(m_value.size() + 1);
		if (m_callbackValueChanged)
			m_callbackValueChanged();
	}
};
