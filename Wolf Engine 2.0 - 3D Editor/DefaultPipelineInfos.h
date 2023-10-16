#pragma once

#include <DescriptorSetLayoutGenerator.h>

namespace DefaultPipeline
{
	struct MatricesUBData
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
	};

	extern Wolf::DescriptorSetLayoutGenerator g_descriptorSetLayoutGenerator;
	extern VkDescriptorSetLayout g_descriptorSetLayout;
}