#pragma once

#include <DescriptorSetBindInfo.h>
#include <DescriptorSetLayoutGenerator.h>
#include <GPUSemaphore.h>
#include <ResourceUniqueOwner.h>
#include <ShaderParser.h>

#include "CommonLayouts.h"

class GlobalIrradiancePassInterface
{
public:
    virtual ~GlobalIrradiancePassInterface() = default;

    virtual const Wolf::Semaphore* getSemaphore() const = 0;
    virtual bool wasEnabledThisFrame() const = 0;

    Wolf::DescriptorSetBindInfo getDescriptorSetToBind()
    {
        return
        {
            m_outputDescriptorSet.createConstNonOwnerResource(),
            m_outputDescriptorSetLayout.createConstNonOwnerResource(),
            DescriptorSetSlots::DESCRIPTOR_SET_SLOT_GLOBAL_IRRADIANCE_INFO
        };
    }

    virtual void addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const = 0;

protected:
    Wolf::DescriptorSetLayoutGenerator m_outputDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_outputDescriptorSet;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_outputDescriptorSetLayout;
};
