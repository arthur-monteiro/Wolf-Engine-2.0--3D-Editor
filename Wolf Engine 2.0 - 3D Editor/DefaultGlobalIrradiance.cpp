#include "DefaultGlobalIrradiance.h"

#include <fstream>

#include <DescriptorSetGenerator.h>

DefaultGlobalIrradiance::DefaultGlobalIrradiance()
{
    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));

    m_outputDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 0); // uniform buffer
    m_outputDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_outputDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_outputDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setUniformBuffer(0, *m_uniformBuffer);

    m_outputDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_outputDescriptorSetLayout));
    m_outputDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void DefaultGlobalIrradiance::update()
{
    UniformBufferData ubData{};
    ubData.ambientIntensity = 0.1f;
    m_uniformBuffer->transferCPUMemory(&ubData, sizeof(UniformBufferData));
}

void DefaultGlobalIrradiance::addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const
{
    std::ifstream inFile("Shaders/defaultGlobalIrradiance/readGlobalIrradiance.glsl");
    std::string line;
    while (std::getline(inFile, line))
    {
        const std::string& descriptorSlotToken = "@VOXEL_GI_DESCRIPTOR_SLOT";
        size_t descriptorSlotTokenPos = line.find(descriptorSlotToken);
        while (descriptorSlotTokenPos != std::string::npos)
        {
            line.replace(descriptorSlotTokenPos, descriptorSlotToken.length(), std::to_string(bindingSlot));
            descriptorSlotTokenPos = line.find(descriptorSlotToken);
        }
        inOutShaderCodeToAdd.codeString += line + '\n';
    }
}
