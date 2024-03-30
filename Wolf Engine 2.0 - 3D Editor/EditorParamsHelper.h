#pragma once

#include <span>

#include <Debug.h>
#include <JSONReader.h>

#include "EditorParamArray.h"
#include "EditorTypes.h"

class DummyParameterGroup : public ParameterGroupInterface
{
public:
	DummyParameterGroup() : ParameterGroupInterface("Dummy") {}
	DummyParameterGroup(const DummyParameterGroup&) = default;
	std::span<EditorParamInterface*> getAllParams() override { return {}; }
	std::span<EditorParamInterface* const> getAllConstParams() const override { return {}; }
	bool hasDefaultName() const override { return false; }
};

template <typename ArrayItemType = DummyParameterGroup>
inline void loadParams(Wolf::JSONReader& jsonReader, const std::string& objectId, std::span<EditorParamInterface*> params, uint32_t arrayIdx = 0)
{
	Wolf::JSONReader::JSONObjectInterface* root = jsonReader.getRoot()->getPropertyObject(objectId);
	const uint32_t paramCount = root->getArraySize("params");

	auto findParamObject = [&root, paramCount, arrayIdx](const std::string& paramName)
		{
			uint32_t paramFoundCount = 0;
			for (uint32_t i = 0; i < paramCount; ++i)
			{
				Wolf::JSONReader::JSONObjectInterface* paramObject = root->getArrayObjectItem("params", i);
				if (paramObject->getPropertyString("name") == paramName)
				{
					if (paramFoundCount == arrayIdx)
						return paramObject;
					paramFoundCount++;
				}
			}

			return static_cast<Wolf::JSONReader::JSONObjectInterface*>(nullptr);
		};

	for (EditorParamInterface* param : params)
	{
		switch (param->getType()) 
		{
			case EditorParamInterface::Type::Float:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName()))
					{
						*dynamic_cast<EditorParamFloat*>(param) = object->getPropertyFloat("value");
					}
				}
				break;
			case EditorParamInterface::Type::UInt:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName()))
					{
						*dynamic_cast<EditorParamUInt*>(param) = static_cast<uint32_t>(object->getPropertyFloat("value"));
					}
				}
				break;
			case EditorParamInterface::Type::Vector2:
				*static_cast<EditorParamVector2*>(param) = glm::vec2(findParamObject(param->getName())->getPropertyFloat("valueX"), findParamObject(param->getName())->getPropertyFloat("valueY"));
				break;
			case EditorParamInterface::Type::Vector3:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName()))
					{
						*dynamic_cast<EditorParamVector3*>(param) = glm::vec3(object->getPropertyFloat("valueX"), object->getPropertyFloat("valueY"), object->getPropertyFloat("valueZ"));
					}
				}
				break;
			case EditorParamInterface::Type::String:
			case EditorParamInterface::Type::File:
			case EditorParamInterface::Type::Entity:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName()))
					{
						*dynamic_cast<EditorParamString*>(param) = object->getPropertyString("value");
					}
				}
				break;
			case EditorParamInterface::Type::Array:
			{
				const uint32_t count = static_cast<uint32_t>(findParamObject(param->getName())->getPropertyFloat("count"));
				for (uint32_t itemArrayIdx = 0; itemArrayIdx < count; ++itemArrayIdx)
				{
					ArrayItemType& item = static_cast<EditorParamArray<ArrayItemType>*>(param)->emplace_back();
					std::vector<EditorParamInterface*> arrayItemParams;
					arrayItemParams.assign(item.getAllParams().begin(), item.getAllParams().end());
					arrayItemParams.push_back(item.getNameParam());
					loadParams(jsonReader, objectId, arrayItemParams, itemArrayIdx);
				}
				break;
			}
			case EditorParamInterface::Type::Bool:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName()))
					{
						*dynamic_cast<EditorParamBool*>(param) = object->getPropertyBool("value");
					}
				}
				break;
			default:
				Wolf::Debug::sendError("Unsupported type");
				break;
		}
	}
}

inline void addParamsToJSON(std::string& outJSON, std::span<EditorParamInterface* const> params, bool isLast, uint32_t tabCount = 2)
{
	for (uint32_t i = 0; i < params.size(); ++i)
	{
		const EditorParamInterface* param = params[i];
		param->addToJSON(outJSON, tabCount, isLast && i == params.size() - 1);
	}
}
