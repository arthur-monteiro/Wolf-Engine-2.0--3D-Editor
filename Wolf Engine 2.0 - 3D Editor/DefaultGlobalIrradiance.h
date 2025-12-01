#pragma once

#include <glm/glm.hpp>

#include <UniformBuffer.h>

#include "GlobalIrradiancePassInterface.h"

class DefaultGlobalIrradiance : public GlobalIrradiancePassInterface
{
public:
    DefaultGlobalIrradiance();

    void update();

    [[nodiscard]] bool wasEnabledThisFrame() const override { return false; }
    [[nodiscard]] const Wolf::Semaphore* getSemaphore() const override { return nullptr; }
    void addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const override;

private:
    struct UniformBufferData
    {
        float ambientIntensity;;
        glm::vec3 padding;
    };
    Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;
};
