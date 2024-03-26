#pragma once

#include <functional>
#include <string>
#include <glm/glm.hpp>

#include <WolfEngine.h>

namespace ultralight
{
	class JSArgs;
	class JSObject;
}

namespace Wolf
{
	class WolfEngine;
}

class EditorParamInterface
{
public:
	virtual ~EditorParamInterface() = default;

	static void setGlobalWolfInstance(Wolf::WolfEngine* wolfInstance);

	virtual void activate();
	virtual void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const = 0;

	void setCategory(const std::string& category) { m_category = category; }

	enum class Type { Float, Vector2, Vector3, String, UInt, File, Array, Entity, Bool };
	Type getType() const { return m_type; }
	const std::string& getName() const { return m_name; }
	bool isEnabled() const { return !m_isActivable || m_isEnabled; }

protected:
	EditorParamInterface(Type type, std::string name, std::string tab, std::string category, bool isActivable) : m_name(std::move(name)), m_tab(std::move(tab)), m_category(std::move(category)),
		m_isActivable(isActivable), m_type(type) {}

	static Wolf::WolfEngine* ms_wolfInstance;
	
	std::string m_name;
	std::string m_tab;
	std::string m_category;
	bool m_isActivable;
	std::function<void()> m_callbackValueChanged;
	Type m_type;

	void addCommonInfoToJSON(std::string& out, uint32_t tabCount) const;
	std::string getTypeAsString() const;

	static std::string removeSpaces(const std::string& in);

private:
	void disableParamJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void enabledParamJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	bool m_isEnabled = false;
};

template <typename T>
class EditorParamsVector : public EditorParamInterface
{
public:
	EditorParamsVector() = delete;
	EditorParamsVector(EditorParamsVector&) = delete;
	EditorParamsVector(EditorParamsVector&&) = delete;

	const T& getValue() const { return m_value; }
	T& getValue() { return m_value; }

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override;

protected:
	EditorParamsVector(Type type, const std::string& name, const std::string& tab, const std::string& category, float min, float max, bool isActivable) : EditorParamInterface(type, name, tab, category, isActivable)
	{
		m_min = min;
		m_max = max;
	}
	EditorParamsVector(Type type, const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged, bool isActivable)
		: EditorParamsVector(type, name, tab, category, min, max, isActivable)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	float m_min = 0.0f;
	float m_max = 1.0f;

	T m_value;
	void setValue(const T& value);
	void setValue(float value, uint32_t componentIdx);
	void setValueY(float valueY) { m_value.y = valueY; }
	void setValueZ(float valueZ) { m_value.z = valueZ; }

	void setValueXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void setValueYJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void setValueZJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
};

class EditorParamVector2 : public EditorParamsVector<glm::vec2>
{
public:
	EditorParamVector2(const std::string& name, const std::string& tab, const std::string& category, float min, float max, bool isActivable = false) : EditorParamsVector(Type::Vector2, name, tab, category, min, max, isActivable) {}
	EditorParamVector2(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamsVector(Type::Vector2, name, tab, category, min, max, callbackValueChanged, isActivable) {}

	EditorParamVector2& operator=(const glm::vec2& value) { setValue(value); return *this; }
	operator glm::vec2& () { return m_value; }
};

class EditorParamVector3 : public EditorParamsVector<glm::vec3>
{
public:
	EditorParamVector3(const std::string& name, const std::string& tab, const std::string& category, float min, float max, bool isActivable = false) : EditorParamsVector(Type::Vector3, name, tab, category, min, max, isActivable) {}
	EditorParamVector3(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamsVector(Type::Vector3, name, tab, category, min, max, callbackValueChanged, isActivable) {}

	EditorParamVector3& operator=(const glm::vec3& value) { setValue(value); return *this; }
	operator glm::vec3& () { return m_value; }
	operator glm::vec3 () const { return m_value; }
};

class EditorParamUInt : public EditorParamInterface
{
public:
	EditorParamUInt(const std::string& name, const std::string& tab, const std::string& category, uint32_t min, uint32_t max, bool isActivable = false) : EditorParamInterface(Type::UInt, name, tab, category, isActivable)
	{
		m_min = min;
		m_max = max;
	}
	EditorParamUInt(const std::string& name, const std::string& tab, const std::string& category, uint32_t min, uint32_t max, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamUInt(name, tab, category, min, max, isActivable)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override;

	EditorParamUInt& operator=(uint32_t value) { setValue(value); return *this; }
	operator uint32_t() const { return m_value; }

private:
	void setValue(uint32_t value);
	void setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	uint32_t m_value = 0;
	uint32_t m_min;
	uint32_t m_max;
};

class EditorParamFloat : public EditorParamInterface
{
public:
	EditorParamFloat(const std::string& name, const std::string& tab, const std::string& category, float min, float max, bool isActivable = false) : EditorParamInterface(Type::Float, name, tab, category, isActivable)
	{
		m_min = min;
		m_max = max;
	}
	EditorParamFloat(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamFloat(name, tab, category, min, max, isActivable)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override;

	EditorParamFloat& operator=(float value) { setValue(value); return *this; }
	operator float() const { return m_value; }

private:
	void setValue(float value);
	void setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	float m_value = 0;
	float m_min;
	float m_max;
};

class EditorParamString : public EditorParamInterface
{
public:
	enum class ParamStringType { STRING, FILE, ENTITY };
	EditorParamString(const std::string& name, const std::string& tab, const std::string& category, ParamStringType stringType = ParamStringType::STRING, bool drivesCategoryName = false, bool isActivable = false)
		: EditorParamInterface(stringTypeToParamType(stringType), name, tab, category, isActivable), m_drivesCategoryName(drivesCategoryName) {}
	EditorParamString(const std::string& name, const std::string& tab, const std::string& category, const std::function<void()>& callbackValueChanged, ParamStringType stringType = ParamStringType::STRING, bool drivesCategoryName = false, bool isActivable = false)
		: EditorParamString(name, tab, category, stringType, drivesCategoryName, isActivable)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override;

	EditorParamString& operator=(const std::string& value) { setValue(value); return *this; }
	operator std::string& () { return m_value; }
	operator const std::string& () const { return m_value; }

private:
	static Type stringTypeToParamType(ParamStringType stringType);
	void setValue(const std::string& value);
	void setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	bool m_drivesCategoryName;
	std::string m_value;
};

class EditorParamBool : public EditorParamInterface
{
public:
	EditorParamBool(const std::string& name, const std::string& tab, const std::string& category) : EditorParamInterface(Type::Bool, name, tab, category, false) {}
	EditorParamBool(const std::string& name, const std::string& tab, const std::string& category, const std::function<void()>& callbackValueChanged) : EditorParamInterface(Type::Bool, name, tab, category, false)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override;

	EditorParamBool& operator=(bool value) { setValue(value); return *this; }
	operator bool() const { return m_value; }

private:
	void setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void setValue(bool value);

	bool m_value = false;
};