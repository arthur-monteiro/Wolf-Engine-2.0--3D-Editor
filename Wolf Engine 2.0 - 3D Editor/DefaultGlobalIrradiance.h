#pragma once

#include "GlobalIrradiancePassInterface.h"

class DefaultGlobalIrradiance : public GlobalIrradiancePassInterface
{
public:
    [[nodiscard]] bool wasEnabledThisFrame() const override { return false; }
    [[nodiscard]] const Wolf::Semaphore* getSemaphore() const override { return nullptr; }
    void addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const override;
};
