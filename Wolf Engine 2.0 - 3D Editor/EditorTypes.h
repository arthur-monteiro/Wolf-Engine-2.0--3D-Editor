#pragma once

#include <functional>
#include <string>
#include <glm/glm.hpp>

#include <JSONReader.h>
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

	enum class Type { Float, Vector2, Vector3, String, UInt, File, Array, Entity, Bool, Enum, Group, Curve };
	Type getType() const { return m_type; }
	const std::string& getName() const { return m_name; }
	const std::string& getCategory() const { return m_category; }
	bool isEnabled() const { return !m_isActivable || m_isEnabled; }

protected:
	EditorParamInterface(Type type, std::string name, std::string tab, std::string category, bool isActivable, bool isReadOnly) : m_name(std::move(name)), m_tab(std::move(tab)), m_category(std::move(category)),
		m_isActivable(isActivable), m_isReadOnly(isReadOnly), m_type(type) {}

	static Wolf::WolfEngine* ms_wolfInstance;
	
	std::string m_name;
	std::string m_tab;
	std::string m_category;
	bool m_isActivable;
	bool m_isReadOnly;
	std::function<void()> m_callbackValueChanged;
	Type m_type;

	void addCommonInfoToJSON(std::string& out, uint32_t tabCount) const;
	std::string getTypeAsString() const;

	static std::string formatStringForFunctionName(const std::string& in);

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
	EditorParamsVector(Type type, const std::string& name, const std::string& tab, const std::string& category, float min, float max, bool isActivable, bool isReadOnly) : EditorParamInterface(type, name, tab, category, isActivable, isReadOnly)
	{
		m_min = min;
		m_max = max;
	}
	EditorParamsVector(Type type, const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged, bool isActivable, bool isReadOnly)
		: EditorParamsVector(type, name, tab, category, min, max, isActivable, isReadOnly)
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
	EditorParamVector2(const std::string& name, const std::string& tab, const std::string& category, float min, float max, bool isActivable = false, bool isReadOnly = false)
		: EditorParamsVector(Type::Vector2, name, tab, category, min, max, isActivable, isReadOnly) {}
	EditorParamVector2(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamsVector(Type::Vector2, name, tab, category, min, max, callbackValueChanged, isActivable, false) {}

	EditorParamVector2& operator=(const glm::vec2& value) { setValue(value); return *this; }
	operator glm::vec2& () { return m_value; }
};

class EditorParamVector3 : public EditorParamsVector<glm::vec3>
{
public:
	EditorParamVector3(const std::string& name, const std::string& tab, const std::string& category, float min, float max, bool isActivable = false, bool isReadOnly = false)
		: EditorParamsVector(Type::Vector3, name, tab, category, min, max, isActivable, isReadOnly) {}
	EditorParamVector3(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamsVector(Type::Vector3, name, tab, category, min, max, callbackValueChanged, isActivable, false) {}

	EditorParamVector3& operator=(const glm::vec3& value) { setValue(value); return *this; }
	operator glm::vec3& () { return m_value; }
	operator glm::vec3 () const { return m_value; }
};

class EditorParamUInt : public EditorParamInterface
{
public:
	EditorParamUInt(const std::string& name, const std::string& tab, const std::string& category, uint32_t min, uint32_t max, bool isActivable = false, bool isReadOnly = false)
		: EditorParamInterface(Type::UInt, name, tab, category, isActivable, isReadOnly)
	{
		m_min = min;
		m_max = max;
	}
	EditorParamUInt(const std::string& name, const std::string& tab, const std::string& category, uint32_t min, uint32_t max, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamUInt(name, tab, category, min, max, isActivable, false)
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
	EditorParamFloat(const std::string& name, const std::string& tab, const std::string& category, float min, float max, bool isActivable = false, bool isReadOnly = false)
		: EditorParamInterface(Type::Float, name, tab, category, isActivable, isReadOnly)
	{
		m_min = min;
		m_max = max;

		m_value = min;
	}
	EditorParamFloat(const std::string& name, const std::string& tab, const std::string& category, float min, float max, const std::function<void()>& callbackValueChanged, bool isActivable = false)
		: EditorParamFloat(name, tab, category, min, max, isActivable, false)
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
	enum class ParamStringType { STRING, FILE_OBJ, FILE_IMG, ENTITY };
	EditorParamString(const std::string& name, const std::string& tab, const std::string& category, ParamStringType stringType = ParamStringType::STRING, bool drivesCategoryName = false, bool isActivable = false, bool isReadOnly = false)
		: EditorParamInterface(stringTypeToParamType(stringType), name, tab, category, isActivable, isReadOnly), m_stringType(stringType), m_drivesCategoryName(drivesCategoryName) {}
	EditorParamString(const std::string& name, const std::string& tab, const std::string& category, const std::function<void()>& callbackValueChanged, ParamStringType stringType = ParamStringType::STRING, bool drivesCategoryName = false, bool isActivable = false)
		: EditorParamString(name, tab, category, stringType, drivesCategoryName, isActivable, false)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override;

	EditorParamString& operator=(const std::string& value) { setValue(value); return *this; }
	operator std::string& () { return m_value; }
	operator const std::string& () const { return m_value; }

	bool drivesCategoryName() const { return m_drivesCategoryName; }

private:
	static Type stringTypeToParamType(ParamStringType stringType);
	void setValue(const std::string& value);
	void setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	ParamStringType m_stringType;
	bool m_drivesCategoryName;
	std::string m_value;
};

class EditorParamBool : public EditorParamInterface
{
public:
	EditorParamBool(const std::string& name, const std::string& tab, const std::string& category, bool isReadOnly = false) : EditorParamInterface(Type::Bool, name, tab, category, false, isReadOnly) {}
	EditorParamBool(const std::string& name, const std::string& tab, const std::string& category, const std::function<void()>& callbackValueChanged) : EditorParamInterface(Type::Bool, name, tab, category, false, false)
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

class EditorParamEnum : public EditorParamInterface
{
public:
	EditorParamEnum(const std::vector<std::string>& options, const std::string& name, const std::string& tab, const std::string& category, bool isActivable = false, bool isReadOnly = false)
		: EditorParamInterface(Type::Enum, name, tab, category, isActivable, isReadOnly)
	{
		m_options = options;
	}
	EditorParamEnum(const std::vector<std::string>& options, const std::string& name, const std::string& tab, const std::string& category, const std::function<void()>& callbackValueChanged, bool isActivable = false, bool isReadOnly = false)
		: EditorParamEnum(options, name, tab, category, isActivable, isReadOnly)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override;

	EditorParamEnum& operator=(uint32_t value) { setValue(value); return *this; }
	operator uint32_t& () { return m_value; }
	operator uint32_t () const { return m_value; }

private:
	void setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);
	void setValue(uint32_t value);

	uint32_t m_value = 0;
	std::vector<std::string> m_options;
};

class EditorParamCurve : public EditorParamInterface
{
public:
	EditorParamCurve(const std::string& name, const std::string& tab, const std::string& category, bool isActivable = false, bool isReadOnly = false)
		: EditorParamInterface(Type::Curve, name, tab, category, isActivable, isReadOnly)
	{
		m_lines.resize(1);
		m_lines[0] = { glm::vec2(0.0f, 0.0f) };
	}
	EditorParamCurve(const std::string& name, const std::string& tab, const std::string& category, const std::function<void()>& callbackValueChanged, bool isActivable = false, bool isReadOnly = false)
		: EditorParamCurve(name, tab, category, isActivable, isReadOnly)
	{
		m_callbackValueChanged = callbackValueChanged;
	}

	void activate() override;
	void addToJSON(std::string& out, uint32_t tabCount, bool isLast) const override;

	void computeValues(std::vector<float>& outValues, uint32_t valueCount) const;

	void loadData(Wolf::JSONReader::JSONObjectInterface* object);

private:
	void setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args);

	struct LineData
	{
		glm::vec2 startPoint;
	};
	std::vector<LineData> m_lines;
	float m_endPointY = 1.0f;
};