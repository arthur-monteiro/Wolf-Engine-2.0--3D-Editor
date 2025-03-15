#pragma once

#include <span>

#include <Debug.h>
#include <JSONReader.h>

#include "EditorTypesTemplated.h"
#include "EditorTypes.h"

class DummyParameterGroup final : public ParameterGroupInterface
{
public:
	DummyParameterGroup() : ParameterGroupInterface("Dummy") {}
	DummyParameterGroup(const DummyParameterGroup&) = default;
	void getAllParams(std::vector<EditorParamInterface*>& out) const override {}
	void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override {}
	bool hasDefaultName() const override { return false; }
};

template <typename GroupItemType = DummyParameterGroup>
inline void loadParams(Wolf::JSONReader& jsonReader, const std::string& objectId, std::span<EditorParamInterface*> params, bool loadArrayItems = true)
{
	Wolf::JSONReader::JSONObjectInterface* root = jsonReader.getRoot()->getPropertyObject(objectId);
	const uint32_t paramCount = root->getArraySize("params");

	auto findParamObject = [&root, paramCount](const std::string& paramName, const std::string& paramCategory)
		{
			for (uint32_t i = 0; i < paramCount; ++i)
			{
				Wolf::JSONReader::JSONObjectInterface* paramObject = root->getArrayObjectItem("params", i);
				if (paramObject->getPropertyString("name") == paramName && (paramCategory.empty() || paramObject->getPropertyString("category") == paramCategory) && !paramObject->hasBeenVisited())
				{
					paramObject->setVisited();
					return paramObject;
				}
			}

			return static_cast<Wolf::JSONReader::JSONObjectInterface*>(nullptr);
		};

	// First change category depending on strings driving category name
	bool stringDrivingCategoryNameAlreadyFound = false;
	for (EditorParamInterface* param : params)
	{
		if (param->getType() == EditorParamInterface::Type::STRING)
		{
			if (dynamic_cast<EditorParamString*>(param)->drivesCategoryName())
			{
				if (stringDrivingCategoryNameAlreadyFound)
				{
					Wolf::Debug::sendError("Only one string driving category name can be there, otherwise we can't know which string needs which value");
				}
				else if (Wolf::JSONReader::JSONObjectInterface * object = findParamObject(param->getName(), ""))
				{
					*dynamic_cast<EditorParamString*>(param) = object->getPropertyString("value"); // will call callbacks changing the category for all properties concerned
				}
				stringDrivingCategoryNameAlreadyFound = true;
			}
		}
	}

	for (EditorParamInterface* param : params)
	{
		switch (param->getType()) 
		{
			case EditorParamInterface::Type::FLOAT:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
					{
						*dynamic_cast<EditorParamFloat*>(param) = object->getPropertyFloat("value");
					}
				}
				break;
			case EditorParamInterface::Type::UINT:
			case EditorParamInterface::Type::TIME:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
					{
						*dynamic_cast<EditorParamUInt*>(param) = static_cast<uint32_t>(object->getPropertyFloat("value"));
					}
				}
				break;
			case EditorParamInterface::Type::VECTOR2:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
					{
						*dynamic_cast<EditorParamVector2*>(param) = glm::vec2(object->getPropertyFloat("valueX"), object->getPropertyFloat("valueY"));
					}
				}
				break;
			case EditorParamInterface::Type::VECTOR3:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
					{
						*dynamic_cast<EditorParamVector3*>(param) = glm::vec3(object->getPropertyFloat("valueX"), object->getPropertyFloat("valueY"), object->getPropertyFloat("valueZ"));
					}
				}
				break;
			case EditorParamInterface::Type::STRING:
			case EditorParamInterface::Type::FILE:
			case EditorParamInterface::Type::ENTITY:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
					{
						*dynamic_cast<EditorParamString*>(param) = object->getPropertyString("value");
						if (param->getType() == EditorParamInterface::Type::ENTITY)
						{
							dynamic_cast<EditorParamString*>(param)->setNoEntitySelectedString(object->getPropertyString("noEntitySelectedName"));
						}
					}
				}
				break;
			case EditorParamInterface::Type::ARRAY:
			{
				if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
				{
					const uint32_t count = static_cast<uint32_t>(object->getPropertyFloat("count"));
					for (uint32_t itemArrayIdx = 0; itemArrayIdx < count; ++itemArrayIdx)
					{
						GroupItemType& item = static_cast<EditorParamArray<GroupItemType>*>(param)->emplace_back();
						if (loadArrayItems)
							item.loadParams(jsonReader, objectId);
					}
				}
				break;
			}
			case EditorParamInterface::Type::BOOL:
				{
					if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
					{
						*dynamic_cast<EditorParamBool*>(param) = object->getPropertyBool("value");
					}
				}
				break;
			case EditorParamInterface::Type::GROUP:
			{
				GroupItemType& item = static_cast<EditorParamGroup<GroupItemType>*>(param)->get();
				std::vector<EditorParamInterface*> arrayItemParams;
				item.getAllParams(arrayItemParams);
				arrayItemParams.push_back(item.getNameParam());
				loadParams(jsonReader, objectId, arrayItemParams);

				break;
			}
			case EditorParamInterface::Type::CURVE:
			{
				if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
				{
					dynamic_cast<EditorParamCurve*>(param)->loadData(object);
				}
				break;
			}
			case EditorParamInterface::Type::ENUM:
			{
				if (Wolf::JSONReader::JSONObjectInterface* object = findParamObject(param->getName(), param->getCategory()))
				{
					*dynamic_cast<EditorParamEnum*>(param) = static_cast<uint32_t>(object->getPropertyFloat("value"));
				}
				break;
			}
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