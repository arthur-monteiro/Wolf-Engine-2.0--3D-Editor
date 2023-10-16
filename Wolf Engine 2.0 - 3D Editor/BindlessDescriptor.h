#pragma once

#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Sampler.h>

namespace Wolf
{
	class Image;
}

class BindlessDescriptor
{
public:
	static constexpr uint32_t DESCRIPTOR_SLOT = 0;

	BindlessDescriptor();

	[[nodiscard]] uint32_t addImages(const std::vector<Wolf::Image*>& images);

	void bind(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
	VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout->getDescriptorSetLayout(); }

private:
	static constexpr uint32_t MAX_IMAGES = 1000;
	static constexpr uint32_t BINDING_SLOT = 0;

	std::unique_ptr<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
	std::unique_ptr<Wolf::DescriptorSet> m_descriptorSet;

	std::unique_ptr<Wolf::Sampler> m_sampler;

	uint32_t m_currentCounter = 0;
};

