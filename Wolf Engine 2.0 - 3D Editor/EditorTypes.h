#pragma once

#include <functional>
#include <string>
#include <glm/glm.hpp>

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
	EditorParamInterface(EditorParamInterface&) = delete;
	EditorParamInterface(EditorParamInterface&&) = delete;
	EditorParamInterface& operator=(EditorParamInterface& other) = delete;
	EditorParamInterface& operator=(EditorParamInterface&& other) = delete;

	static void setGlobalWolfInstance(Wolf::WolfEngine* wolfInstance);

	virtual void activate() = 0;
	virtual void addToJSON(std::string& out, uint32_t tabCount, bool isLast) = 0;

protected:
	enum class Type { Float, Vector2, Vector3, String, UInt, File };
	EditorParamInterface(Type type, std::string name, std::string tab, std::string category) : m_name(std::move(name)), m_tab(std::move(tab)), m_category(std::move(category)), m_type(type) {}

	static Wolf::WolfEngine* ms_wolfInstance;
	
	std::string m_name;
	std::string m_tab;
	std::string m_category;
	std::function<void()> m_callbackValueChanged;

	void addCommonInfoToJSON(std::string& out, uint32_t tabCount);
	std::string getTypeAsString();

	static std::string removeSpaces(const std::string& in);

private:
	Type m_type;
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
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) override;

protected:
	EditorParamsVector(Type type, const std::string& name, const std::string& tab, const std::string& category, float min, float max) : EditorParamInterface(type, name, tab, category)
	{
		m_min = min;
		m_max = max;
	}
	EditorParamsVector(Type type, const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged)
		: EditorParamsVector(type, name, tab, category, min, max)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	float m_min;
	float m_max;

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
	EditorParamVector2(const std::string& name, const std::string& tab, const std::string& category, float min, float max) : EditorParamsVector(Type::Vector2, name, tab, category, min, max) {}
	EditorParamVector2(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged) : EditorParamsVector(Type::Vector2, name, tab, category, min, max, callbackValueChanged) {}

	EditorParamVector2& operator=(const glm::vec2& value) { setValue(value); return *this; }
	operator glm::vec2& () { return m_value; }
};

class EditorParamVector3 : public EditorParamsVector<glm::vec3>
{
public:
	EditorParamVector3(const std::string& name, const std::string& tab, const std::string& category, float min, float max) : EditorParamsVector(Type::Vector3, name, tab, category, min, max) {}
	EditorParamVector3(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged) : EditorParamsVector(Type::Vector3, name, tab, category, min, max, callbackValueChanged) {}

	EditorParamVector3& operator=(const glm::vec3& value) { setValue(value); return *this; }
	operator glm::vec3& () { return m_value; }
};

class EditorParamUInt : public EditorParamInterface
{
public:
	EditorParamUInt(const std::string& name, const std::string& tab, const std::string& category, uint32_t min, uint32_t max) : EditorParamInterface(Type::UInt, name, tab, category)
	{
		m_min = min;
		m_max = max;
	}
	EditorParamUInt(const std::string& name, const std::string& tab, const std::string& category, uint32_t min, uint32_t max, const std::function<void()>& callbackValueChanged) : EditorParamUInt(name, tab, category, min, max)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) override;

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
	EditorParamFloat(const std::string& name, const std::string& tab, const std::string& category, float min, float max) : EditorParamInterface(Type::Float, name, tab, category)
	{
		m_min = min;
		m_max = max;
	}
	EditorParamFloat(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged) : EditorParamFloat(name, tab, category, min, max)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) override;

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
	EditorParamString(const std::string& name, const std::string& tab, const std::string& category, bool isFile = false) : EditorParamInterface(isFile? Type::File : Type::String, name, tab, category) {}
	EditorParamString(const std::string& name, const std::string& tab, const std::string& category, const std::function<void()>& callbackValueChanged, bool isFile = false) : EditorParamString(name, tab, category, isFile)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) override;

	EditorParamString& operator=(const std::string& value) { setValue(value); return *this; }
	operator std::string& () { return m_value; }
	operator const std::string& () const { return m_value; }

private:
	void setValue(const std::string& value);
	void setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	std::string m_value;
};