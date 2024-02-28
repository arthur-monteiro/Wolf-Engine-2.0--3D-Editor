#pragma once

#include <fstream>
#include <span>

#include <JSONReader.h>

#include "EditorTypes.h"

inline void loadParams(Wolf::JSONReader& jsonReader, const std::string& objectId, std::span<EditorParamInterface*> params)
{
	Wolf::JSONReader::JSONObjectInterface* root = jsonReader.getRoot()->getPropertyObject(objectId);
	const uint32_t paramCount = root->getArraySize("params");

	auto findParamObject = [&root, paramCount](const std::string& paramName)
		{
			for (uint32_t i = 0; i < paramCount; ++i)
			{
				Wolf::JSONReader::JSONObjectInterface* paramObject = root->getArrayObjectItem("params", i);
				if (paramObject->getPropertyString("name") == paramName)
				{
					return paramObject;
				}
			}

			return static_cast<Wolf::JSONReader::JSONObjectInterface*>(nullptr);
		};

	for (EditorParamInterface* param : params)
	{
		switch (param->getType()) 
		{
			case EditorParamInterface::Type::Float:
				*static_cast<EditorParamFloat*>(param) = findParamObject(param->getName())->getPropertyFloat("value");
				break;
			case EditorParamInterface::Type::UInt:
				*static_cast<EditorParamUInt*>(param) = static_cast<uint32_t>(findParamObject(param->getName())->getPropertyFloat("value"));
				break;
			case EditorParamInterface::Type::Vector2:
				*static_cast<EditorParamVector2*>(param) = glm::vec2(findParamObject(param->getName())->getPropertyFloat("valueX"), findParamObject(param->getName())->getPropertyFloat("valueY"));
				break;
			case EditorParamInterface::Type::Vector3: 
				*static_cast<EditorParamVector3*>(param) = glm::vec3(findParamObject(param->getName())->getPropertyFloat("valueX"), findParamObject(param->getName())->getPropertyFloat("valueY"),
					findParamObject(param->getName())->getPropertyFloat("valueZ"));
				break;
			case EditorParamInterface::Type::String:
			case EditorParamInterface::Type::File:
				*static_cast<EditorParamString*>(param) = findParamObject(param->getName())->getPropertyString("value");
				break;
			default:
				Wolf::Debug::sendError("Unsupported type");
				break;
		}
	}
}

inline void addParamsToJSON(std::string& outJSON, std::span<EditorParamInterface*> params, bool isLast, uint32_t tabCount = 2)
{
	for (uint32_t i = 0; i < params.size(); ++i)
	{
		EditorParamInterface* param = params[i];
		param->addToJSON(outJSON, tabCount, isLast && i == params.size() - 1);
	}
}
