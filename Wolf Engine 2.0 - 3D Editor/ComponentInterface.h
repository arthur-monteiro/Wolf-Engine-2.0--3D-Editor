#pragma once

#include <string>

#include <ResourceNonOwner.h>

namespace Wolf
{
	class JSONReader;
}

class Entity;

class ComponentInterface
{
public:
	virtual ~ComponentInterface() = default;

	virtual void loadParams(Wolf::JSONReader& jsonReader) = 0;

	virtual void activateParams() = 0;
	virtual void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) = 0;
	
	virtual std::string getId() const = 0;

protected:
	ComponentInterface() {}
};

