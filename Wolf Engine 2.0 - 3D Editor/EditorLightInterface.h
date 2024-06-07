#pragma once

#include "ComponentInterface.h"
#include "LightManager.h"

class EditorLightInterface : public ComponentInterface
{
public:
	virtual void addLightsToLightManager(const Wolf::ResourceNonOwner<LightManager>& lightManager) const = 0;
};