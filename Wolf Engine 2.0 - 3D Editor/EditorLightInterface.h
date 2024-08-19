#pragma once

#include <LightManager.h>

#include "ComponentInterface.h"

class EditorLightInterface : public ComponentInterface
{
public:
	virtual void addLightsToLightManager(const Wolf::ResourceNonOwner<Wolf::LightManager>& lightManager) const = 0;
};