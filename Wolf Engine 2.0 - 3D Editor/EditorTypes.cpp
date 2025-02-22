#include "EditorTypes.h"

#include "JSONReader.h"

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

		const std::string functionDisableName = "disable" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
		jsObject[functionDisableName.c_str()] = std::bind(&EditorParamInterface::disableParamJSCallback, this, std::placeholders::_1, std::placeholders::_2);

		const std::string functionEnableName = "enable" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
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
	out += tabs + R"("isReadOnly" : )" + (m_isReadOnly ? "true" : "false") + ",\n";
}

std::string EditorParamInterface::getTypeAsString() const
{
	switch (m_type)
	{
		case Type::FLOAT:
			return "Float";
		case Type::VECTOR2:
			return "Vector2";
		case Type::VECTOR3: 
			return "Vector3";
		case Type::STRING:
			return "String";
		case Type::UINT:
			return "UInt";
		case Type::FILE:
			return "File";
		case Type::ARRAY:
			return "Array";
		case Type::ENTITY:
			return "Entity";
		case Type::BOOL:
			return "Bool";
		case Type::ENUM:
			return "Enum";
		case Type::GROUP:
			return "Group";
		case Type::CURVE:
			return "Curve";
		case Type::TIME:
			return "Time";
		case Type::BUTTON:
			return "Button";
		default:
			Wolf::Debug::sendError("Undefined type");
			return "Undefined_type";
	}
}

std::string EditorParamInterface::formatStringForFunctionName(const std::string& in)
{
	std::string out;
	bool nextCharIsUpper = false;
	for (const char character : in)
	{
		if (character == ' ' || character == '(' || character == ')' || character == '-')
		{
			nextCharIsUpper = true;
		}
		else if (character == '/')
		{
			out += "_";
		}
		else
		{
			out += nextCharIsUpper ? std::toupper(character) : character;
			nextCharIsUpper = false;
		}
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

	const std::string functionPrefix = "change" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);

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
	if (value != m_value)
	{
		m_value = value;
		if (m_callbackValueChanged)
			m_callbackValueChanged();
	}

	
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

	const std::string functionChangeName = "change" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
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

EditorParamInterface::Type EditorParamUInt::uintTypeToParamType(ParamUIntType uintType)
{
	switch (uintType)
	{
		case ParamUIntType::NUMBER:
			return Type::UINT;
		case ParamUIntType::TIME:
			return Type::TIME;
		default:
			Wolf::Debug::sendError("Unsupported type");
			return Type::UINT;
	}
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

	const std::string functionChangeName = "change" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
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
	
	const std::string functionChangeName = "change" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
	jsObject[functionChangeName.c_str()] = std::bind(&EditorParamString::setValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void EditorParamString::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + +"{\n";
	addCommonInfoToJSON(out, tabCount + 1);
	out += tabs + '\t' + R"("value" : ")" + m_value + "\",\n";
	out += tabs + '\t' + R"("drivesCategoryName" : )" + (m_drivesCategoryName ? "true" : "false") + ",\n";
	out += tabs + '\t' + R"("fileFilter" : ")";
	switch (m_stringType)
	{
		case ParamStringType::STRING:
			break;
		case ParamStringType::FILE_OBJ: 
			out += "obj";
			break;
		case ParamStringType::FILE_DAE:
			out += "dae";
			break;
		case ParamStringType::FILE_IMG: 
			out += "img";
			break;
		case ParamStringType::ENTITY: break;
	}
	out += "\",\n";
	out += tabs + '\t' + R"("noEntitySelectedName" : ")" + m_noEntitySelectedString + "\"\n";
	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

EditorParamInterface::Type EditorParamString::stringTypeToParamType(ParamStringType stringType)
{
	switch (stringType) 
	{
		case ParamStringType::STRING:
			return Type::STRING;
		case ParamStringType::FILE_OBJ:
		case ParamStringType::FILE_IMG:
		case ParamStringType::FILE_DAE:
			return Type::FILE;
		case ParamStringType::ENTITY:
			return Type::ENTITY;
		default:
			Wolf::Debug::sendError("Unhandled string type");
			return Type::STRING;
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

	const std::string functionChangeName = "change" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
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

void EditorParamButton::activate()
{
	EditorParamInterface::activate();

	ultralight::JSObject jsObject;
	ms_wolfInstance->getUserInterfaceJSObject(jsObject);

	const std::string functionChangeName = "change" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
	jsObject[functionChangeName.c_str()] = std::bind(&EditorParamButton::onClickJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void EditorParamButton::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + +"{\n";
	addCommonInfoToJSON(out, tabCount + 1);
	// Remove ',' as there no other property
	out = out.substr(0, out.size() - 2);
	out += "\n";
	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

void EditorParamButton::onClickJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	m_callbackValueChanged();
}

void EditorParamEnum::activate()
{
	EditorParamInterface::activate();

	ultralight::JSObject jsObject;
	ms_wolfInstance->getUserInterfaceJSObject(jsObject);

	const std::string functionChangeName = "change" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
	jsObject[functionChangeName.c_str()] = std::bind(&EditorParamEnum::setValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void EditorParamEnum::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + +"{\n";
	addCommonInfoToJSON(out, tabCount + 1);
	out += tabs + '\t' + R"("options" : [)";
	for (uint32_t i = 0; i < m_options.size(); ++i)
	{
		out += "\"" + m_options[i] + "\"";
		if (i != m_options.size() - 1)
			out += ", ";
	}
	out += "],\n";
	out += tabs + '\t' + R"("value" : )" + std::to_string(m_value) + "\n";
	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

void EditorParamEnum::setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string value = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	for (uint32_t i = 0; i < m_options.size(); ++i)
	{
		if (m_options[i] == value)
		{
			setValue(i);
			return;
		}
	}

	Wolf::Debug::sendError("Value not found in options");
}

void EditorParamEnum::setValue(uint32_t value)
{
	if (m_value != value)
	{
		m_value = value;

		if (m_callbackValueChanged)
			m_callbackValueChanged();
	}
}

void EditorParamCurve::activate()
{
	EditorParamInterface::activate();

	ultralight::JSObject jsObject;
	ms_wolfInstance->getUserInterfaceJSObject(jsObject);

	const std::string functionChangeName = "change" + formatStringForFunctionName(m_tab) + formatStringForFunctionName(m_name) + formatStringForFunctionName(m_category);
	jsObject[functionChangeName.c_str()] = std::bind(&EditorParamCurve::setValueJSCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void EditorParamCurve::addToJSON(std::string& out, uint32_t tabCount, bool isLast) const
{
	std::string tabs;
	for (uint32_t i = 0; i < tabCount; ++i) tabs += '\t';

	out += tabs + +"{\n";
	addCommonInfoToJSON(out, tabCount + 1);

	out += tabs + '\t' + R"("lines" : [)" + '\n';
	for (uint32_t i = 0; i < m_lines.size(); ++i)
	{
		const LineData& line = m_lines[i];

		out += tabs + '\t' + '\t' + R"({ "startPointX" : )" + std::to_string(line.startPoint.x) + ", \"startPointY\" : " + std::to_string(line.startPoint.y) + " }";
		if (i != m_lines.size() - 1)
			out += ',';
		out += '\n';
	}
	out += tabs + '\t' + "],\n";
	out += tabs + '\t' + "\"endPointY\" : " + std::to_string(m_endPointY);

	out += tabs + "}" + (isLast ? "\n" : ",\n");
}

void EditorParamCurve::computeValues(std::vector<float>& outValues, uint32_t valueCount) const
{
	outValues.resize(valueCount);
	for (uint32_t i = 0; i < valueCount; ++i)
	{
		float globalX = static_cast<float>(i) / static_cast<float>(valueCount - 1); // -1 to have last value = last point

		for (uint32_t lineIdx = 0; lineIdx < m_lines.size(); ++lineIdx)
		{
			float lineStartX = m_lines[lineIdx].startPoint.x;
			float lineEndX = lineIdx == m_lines.size() - 1 ? 1.0f : m_lines[lineIdx + 1].startPoint.x;

			if (globalX >= lineStartX && globalX <= lineEndX)
			{
				float localX = (globalX - lineStartX) / (lineEndX - lineStartX);

				float lineStartY = m_lines[lineIdx].startPoint.y;
				float lineEndY = lineIdx == m_lines.size() - 1 ? m_endPointY : m_lines[lineIdx + 1].startPoint.y;

				outValues[i] = glm::mix(lineStartY, lineEndY, localX);
				break;
			}
		}
	}
}

void EditorParamCurve::loadData(Wolf::JSONReader::JSONObjectInterface* object)
{
	uint32_t lineCount = object->getArraySize("lines");
	m_lines.resize(lineCount);

	for (uint32_t i = 0; i < lineCount; ++i)
	{
		Wolf::JSONReader::JSONObjectInterface* lineInfo = object->getArrayObjectItem("lines", i);

		m_lines[i].startPoint = glm::vec2(lineInfo->getPropertyFloat("startPointX"), lineInfo->getPropertyFloat("startPointY"));
	}

	m_endPointY = object->getPropertyFloat("endPointY");
}

void EditorParamCurve::setValueJSCallback(const ultralight::JSObject& thisObject, const ultralight::JSArgs& args)
{
	const std::string jsonValue = static_cast<ultralight::String>(args[0].ToString()).utf8().data();
	Wolf::JSONReader jsonReader(Wolf::JSONReader::StringReadInfo { jsonValue });

	loadData(jsonReader.getRoot());

	if (m_callbackValueChanged)
		m_callbackValueChanged();
}
