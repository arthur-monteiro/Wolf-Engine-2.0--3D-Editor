#include "EditorTypes.h"

Wolf::WolfEngine* EditorParamInterface::ms_wolfInstance = nullptr;

void EditorParamInterface::setGlobalWolfInstance(Wolf::WolfEngine* wolfInstance)
{
	ms_wolfInstance = wolfInstance;
}

void EditorParamInterface::activate()
{
	if (m_isActivable)
	{
		ultralight::JSObject jsObject;
		ms_wolfInstance->getUserInterfaceJSObject(jsObject);

		const std::string functionDisableName = "disable" + removeSpaces(m_tab) + removeSpaces(m_name) + removeSpaces(m_category);
		jsObject[functionDisableName.c_str()] = std::bind(&EditorParamInterface::disableParamJSCallback, this, std::placeholders::_1, std::placeholders::_2);

		const std::string functionEnableName = "enable" + removeSpaces(m_tab) + removeSpaces(m_name) + removeSpaces(m_category);
		jsObject[functionEnableName.c_str()] = std::bind(&EditorParamInterface::enabledParamJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	}
}

void EditorParamInterface::addCommonInfoToJSON(std::string& out, uint32_t tabCount) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + R"("name" : ")" + m_name + "\",\n";
	out += tabs + R"("tab" : ")" + m_tab + "\",\n";
	out += tabs + R"("category" : ")" + m_category + "\",\n";
	out += tabs + R"("type" : ")" + getTypeAsString() + "\",\n";
	out += tabs + R"("isActivable" : )" + (m_isActivable ? "true" : "false") + ",\n";
}

std::string EditorParamInterface::getTypeAsString() const
{
	switch (m_type)
	{
		case Type::Float:
			return "Float";
			break;
		case Type::Vector2:
			return "Vector2";
			break;
		case Type::Vector3: 
			return "Vector3";
			break;
		case Type::String:
			return "String";
			break;
		case Type::UInt:
			return "UInt";
			break;
		case Type::File:
			return "File";
			break;
		case Type::Array:
			return "Array";
			break;
		case Type::Entity:
			return "Entity";
			break;
		case Type::Bool:
			return "Bool";
			break;
		default:
			Wolf::Debug::sendError("Undefined type");
			return "Undefined_type";
	}
}

std::string EditorParamInterface::removeSpaces(const std::string& in)
{
	std::string out;
	bool nextCharIsUpper = false;
	for (const char character : in)
	{
		if (character == ' ')
		{
			nextCharIsUpper = true;
			continue;
		}

		out += nextCharIsUpper ? std::toupper(character) : character;
		nextCharIsUpper = false;
	}
	return out;
}

void EditorParamInterface::disableParamJSCallback(const ultralight::JSObject& thisObject,const ultralight::JSArgs& args)
{
	if (!m_isActivable)
		Wolf::Debug::sendError("Deactivating a param which is not activable");

	if (m_isEnabled)
	{
		m_isEnabled = false;
		if (m_callbackValueChanged)
			m_callbackValueChanged();
	}
}

void EditorParamInterface::enabledParamJSCallback(const ultralight::JSObject& thisObject,
	const ultralight::JSArgs& args)
{
	if (!m_isActivable)
		Wolf::Debug::sendError("Deactivating a param which is not activable");

	if (!m_isEnabled)
	{
		m_isEnabled = true;
		if (m_callbackValueChanged)
			m_callbackValueChanged();
	}
}

template void EditorParamsVector<glm::vec2>::activate();
template void EditorParamsVector<glm::vec3>::activate();

template <typename T>
void EditorParamsVector<T>::activate()
{
	EditorParamInterface::activate();

	ultralight::JSObject jsObject;
	ms_wolfInstance->getUserInterfaceJSObject(jsObject);

	const std::string functionPrefix = "change" + removeSpaces(m_tab) + removeSpaces(m_name) + removeSpaces(m_category);

	const std::string functionChangeXName = functionPrefix + "X";
	jsObject[functionChangeXName.c_str()] = std::bind(&EditorParamsVector::setValueXJSCallback, this, std::placeholders::_1, std::placeholders::_2);

	const std::string functionChangeYName = functionPrefix + "Y";
	jsObject[functionChangeYName.c_str()] = std::bind(&EditorParamsVector::setValueYJSCallback, this, std::placeholders::_1, std::placeholders::_2);

	if (sizeof(T) > sizeof(glm::vec2))
	{
		const std::string functionChangeZName = functionPrefix + "Z";
		jsObject[functionChangeZName.c_str()] = std::bind(&EditorParamsVector::setValueZJSCallback, this, std::placeholders::_1, std::placeholders::_2);
	}
}

template void EditorParamsVector<glm::vec2>::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const;
template void EditorParamsVector<glm::vec3>::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const;

template <typename T>
void EditorParamsVector<T>::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + + "{\n";
	addCommonInfoToJSON(out, tabCount + 1);
	out += tabs + '\t' + R"("valueX" : )" + std::to_string(m_value.x) + ",\n";
	out += tabs + '\t' + R"("valueY" : )" + std::to_string(m_value.y) + ",\n";
	if (sizeof(T) > sizeof(glm::vec2))
		out += tabs + '\t' + R"("valueZ" : )" + std::to_string(m_value[2]) + ",\n";
	out += tabs + '\t' + R"("min" : )" + std::to_string(m_min) + ",\n";
	out += tabs + '\t' + R"("max" : )" + std::to_string(m_max) + "\n";
	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

template void EditorParamsVector<glm::vec2>::setValue(const glm::vec2& value);
template void EditorParamsVector<glm::vec3>::setValue(const glm::vec3& value);

template <typename T>
void EditorParamsVector<T>::setValue(const T& value)
{
	m_value = value;
	if (m_callbackValueChanged)
		m_callbackValueChanged();
}

template <typename T>
void EditorParamsVector<T>::setValue(float value, uint32_t componentIdx)
{
	m_value[static_cast<int>(componentIdx)] = value;
	if (m_callbackValueChanged)
		m_callbackValueChanged();
}

template <typename T>
void EditorParamsVector<T>::setValueXJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const float value = static_cast<float>(args[0].ToNumber());
	setValue(value, 0);
}

template <typename T>
void EditorParamsVector<T>::setValueYJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const float value = static_cast<float>(args[0].ToNumber());
	setValue(value, 1);
}

template <typename T>
void EditorParamsVector<T>::setValueZJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const float value = static_cast<float>(args[0].ToNumber());
	setValue(value, 2);
}

void EditorParamUInt::activate()
{
	EditorParamInterface::activate();

	ultralight::JSObject jsObject;
	ms_wolfInstance->getUserInterfaceJSObject(jsObject);

	const std::string functionChangeName = "change" + removeSpaces(m_tab) + removeSpaces(m_name) + removeSpaces(m_category);
	jsObject[functionChangeName.c_str()] = std::bind(&EditorParamUInt::setValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void EditorParamUInt::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + +"{\n";
	addCommonInfoToJSON(out, tabCount + 1);
	out += tabs + '\t' + R"("value" : )" + std::to_string(m_value) + ",\n";
	out += tabs + '\t' + R"("min" : )" + std::to_string(m_min) + ",\n";
	out += tabs + '\t' + R"("max" : )" + std::to_string(m_max) + "\n";
	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

void EditorParamUInt::setValue(uint32_t value)
{
	m_value = value;
	if (m_callbackValueChanged)
		m_callbackValueChanged();
}

void EditorParamUInt::setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const uint32_t value = static_cast<uint32_t>(args[0].ToInteger());
	setValue(value);
}

void EditorParamFloat::activate()
{
	EditorParamInterface::activate();

	ultralight::JSObject jsObject;
	ms_wolfInstance->getUserInterfaceJSObject(jsObject);

	const std::string functionChangeName = "change" + removeSpaces(m_tab) + removeSpaces(m_name) + removeSpaces(m_category);
	jsObject[functionChangeName.c_str()] = std::bind(&EditorParamFloat::setValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void EditorParamFloat::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + +"{\n";
	addCommonInfoToJSON(out, tabCount + 1);
	out += tabs + '\t' + R"("value" : )" + std::to_string(m_value) + ",\n";
	out += tabs + '\t' + R"("min" : )" + std::to_string(m_min) + ",\n";
	out += tabs + '\t' + R"("max" : )" + std::to_string(m_max) + "\n";
	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

void EditorParamFloat::setValue(float value)
{
	m_value = value;
	if (m_callbackValueChanged)
		m_callbackValueChanged();
}

void EditorParamFloat::setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const float value = static_cast<float>(args[0].ToNumber());
	setValue(value);
}

void EditorParamString::activate()
{
	EditorParamInterface::activate();

	ultralight::JSObject jsObject;
	ms_wolfInstance->getUserInterfaceJSObject(jsObject);
	
	const std::string functionChangeName = "change" + removeSpaces(m_tab) + removeSpaces(m_name) + removeSpaces(m_category);
	jsObject[functionChangeName.c_str()] = std::bind(&EditorParamString::setValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void EditorParamString::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + +"{\n";
	addCommonInfoToJSON(out, tabCount + 1);
	out += tabs + '\t' + R"("value" : ")" + m_value + "\",\n";
	out += tabs + '\t' + R"("drivesCategoryName" : )" + (m_drivesCategoryName ? "true" : "false") + "\n";
	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

EditorParamInterface::Type EditorParamString::stringTypeToParamType(ParamStringType stringType)
{
	switch (stringType) 
	{
		case ParamStringType::STRING: 
			return Type::String;
			break;
		case ParamStringType::FILE: 
			return Type::File;
			break;
		case ParamStringType::ENTITY: 
			return Type::Entity;
			break;
		default: 
			Wolf::Debug::sendError("Unhandled string type");
			return Type::String;
	}
}

void EditorParamString::setValue(const std::string& value)
{
	m_value = value;

	if (m_callbackValueChanged)
		m_callbackValueChanged();
}

void EditorParamString::setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string value = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	setValue(value);
}

void EditorParamBool::activate()
{
	EditorParamInterface::activate();

	ultralight::JSObject jsObject;
	ms_wolfInstance->getUserInterfaceJSObject(jsObject);

	const std::string functionChangeName = "change" + removeSpaces(m_tab) + removeSpaces(m_name) + removeSpaces(m_category);
	jsObject[functionChangeName.c_str()] = std::bind(&EditorParamBool::setValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void EditorParamBool::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + +"{\n";
	addCommonInfoToJSON(out, tabCount + 1);
	out += tabs + '\t' + R"("value" : )" + (m_value ? "true" : "false") + "\n";
	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

void EditorParamBool::setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const bool value = args[0].ToBoolean();
	setValue(value);
}

void EditorParamBool::setValue(bool value)
{
	m_value = value;

	if (m_callbackValueChanged)
		m_callbackValueChanged();
}
