#pragma once

#include <DynamicStableArray.h>

#include "EditorTypes.h"
#include "ParameterGroupInterface.h"

template <typename T>
class EditorParamArray : public EditorParamInterface
{
public:
	EditorParamArray(const std::string& name, const std::string& tab, const std::string& category, uint32_t maxSize, bool isActivable = false, bool isReadOnly = false)
		: EditorParamInterface(Type::Array, name, tab, category, isActivable, isReadOnly)
	{
		m_maxSize = maxSize;
	}
	EditorParamArray(const std::string& name, const std::string& tab, const std::string& category, uint32_t maxSize, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamArray(name, tab, category, maxSize, isActivable)
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
		EditorParamInterface::activate();

		ultralight::JSObject jsObject;
		ms_wolfInstance->getUserInterfaceJSObject(jsObject);

		const std::string functionChangeName = "addTo" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
		jsObject[functionChangeName.c_str()] = std::bind(&EditorParamArray<T>::addValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);

		for (uint32_t i = 0; i < m_value.size(); ++i)
		{
			T& value = m_value[i];

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

	void clear() { m_value.clear(); }
	size_t size() const { return m_value.size(); }
	bool empty() const { return m_value.empty(); }
	T& operator[](size_t idx) { return m_value[idx]; }
	T& back() { return m_value.back(); }
	T& emplace_back() { addValueNoCheck(); return back(); }

	void lockAccessElements() { m_value.lockAccessElements(); }
	void unlockAccessElements() { m_value.unlockAccessElements(); }

private:
	uint32_t m_maxSize;
	Wolf::DynamicStableArray<T, 8> m_value;

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

template <typename T>
class EditorParamGroup : public EditorParamInterface
{
public:
	EditorParamGroup(const std::string& name, const std::string& tab, const std::string& category, bool isActivable = false, bool isReadOnly = false)
		: EditorParamInterface(Type::Group, name, tab, category, isActivable, isReadOnly)
	{
	}
	EditorParamGroup(const EditorParamArray<T>& other) : EditorParamInterface(Type::Array, other.m_name, other.m_tab, other.m_category)
	{
		m_value = other.m_value;
	}

	void activate() override
	{
		EditorParamInterface::activate();

		ultralight::JSObject jsObject;
		ms_wolfInstance->getUserInterfaceJSObject(jsObject);

		ParameterGroupInterface* valueAsParameterGroup = static_cast<ParameterGroupInterface*>(&m_value);
		valueAsParameterGroup->activateParams();
	}

	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override
	{
		std::string tabs;
		for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

		const ParameterGroupInterface* valueAsParameterGroup = static_cast<const ParameterGroupInterface*>(&m_value);
		valueAsParameterGroup->addParamsToJSON(out, tabCount, isLast);
	}

	const T& get() const { return m_value; }
	T& get() { return m_value; }

private:
	T m_value;
};