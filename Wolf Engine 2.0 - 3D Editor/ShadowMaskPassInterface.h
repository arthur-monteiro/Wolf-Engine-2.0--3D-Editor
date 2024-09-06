#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "CommonLayouts.h"
#include "DescriptorSet.h"
#include "DescriptorSetBindInfo.h"
#include "DescriptorSetLayoutGenerator.h"
#include "ResourceUniqueOwner.h"
#include "ShaderParser.h"

namespace Wolf
{
	class Image;
	class Semaphore;
}

class ShadowMaskPassInterface
{
public:
	virtual Wolf::Image* getOutput() = 0;
	virtual const Wolf::Semaphore* getSemaphore() const = 0;
	virtual void getConditionalBlocksToEnableWhenReadingMask(std::vector<std::string>& conditionalBlocks) const = 0;
	virtual Wolf::Image* getDenoisingPatternImage() = 0;
	virtual Wolf::Image* getDebugImage() const { return nullptr; }
	virtual bool wasEnabledThisFrame() const = 0;

	Wolf::DescriptorSetBindInfo getMaskDescriptorSetToBind()
	{
		return
		{
			m_outputMaskDescriptorSet.createConstNonOwnerResource(),
			m_outputMaskDescriptorSetLayout.createConstNonOwnerResource(),
			DescriptorSetSlots::DESCRIPTOR_SET_SLOT_SHADOW_MASK_INFO
		};
	}
	Wolf::DescriptorSetBindInfo getShadowComputeDescriptorSetToBind(uint32_t bindingSlot)
	{
		return
		{
			m_outputComputeDescriptorSet.createConstNonOwnerResource(),
			m_outputComputeDescriptorSetLayout.createConstNonOwnerResource(),
			bindingSlot
		};
	}
	virtual void addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const = 0;

protected:
	// Mask resources
	Wolf::DescriptorSetLayoutGenerator m_outputMaskDescriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_outputMaskDescriptorSet;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_outputMaskDescriptorSetLayout;

	// Resources to compute shadow in a custom shader (used for smoke)
	Wolf::DescriptorSetLayoutGenerator m_outputComputeDescriptorSetLayoutGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_outputComputeDescriptorSet;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_outputComputeDescriptorSetLayout;
};