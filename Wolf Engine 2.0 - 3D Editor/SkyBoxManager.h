#pragma once

#include <glm/glm.hpp>

#include "DebugRenderingManager.h"

class SkyBoxManager
{
public:
    // Compute from spherical map
    struct VertexOnlyPosition
    {
        glm::vec3 pos;

        static void getBindingDescription(VkVertexInputBindingDescription& bindingDescription, uint32_t binding)
        {
            bindingDescription.binding = binding;
            bindingDescription.stride = sizeof(VertexOnlyPosition);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        }

        static void getAttributeDescriptions(std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, uint32_t binding)
        {
            const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
            attributeDescriptions.resize(attributeDescriptionCountBefore + 1);

            attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
            attributeDescriptions[attributeDescriptionCountBefore + 0].location = 0;
            attributeDescriptions[attributeDescriptionCountBefore + 0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(VertexOnlyPosition, pos);
        }

        bool operator==(const VertexOnlyPosition& other) const
        {
            return pos == other.pos;
        }
    };

    SkyBoxManager();

    void updateBeforeFrame(Wolf::WolfEngine* wolfInstance, const Wolf::ResourceNonOwner<ComputeSkyCubeMapPass>& skyCubeMapPass);

    void drawSkyBox(const Wolf::CommandBuffer& commandBuffer, const Wolf::RenderPass& renderPass, const Wolf::RecordContext& context);

    [[nodiscard]] const Wolf::ResourceUniqueOwner<Wolf::Mesh>& getCubeMesh() const { return m_cubeMesh; }
    [[nodiscard]] Wolf::ResourceUniqueOwner<Wolf::Image>& getCubeMapImage() { return m_cubeMapImage; }

    void setCubeMapResolution(uint32_t resolution);
    void setOnCubeMapChangedCallback(const std::function<void(Wolf::ResourceUniqueOwner<Wolf::Image>&)>& onCubeMapChangedCallback);

private:
    void createCubeMap();
    void createPipelineIfNeeded(const Wolf::RenderPass& renderPass);
    void updateDescriptorSet();

    Wolf::ResourceUniqueOwner<Wolf::Mesh> m_cubeMesh;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_vertexShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_fragmentShaderParser;

    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_pipeline;

    Wolf::ResourceUniqueOwner<Wolf::Image> m_cubeMapImage;
    uint32_t m_cubeMapResolution = 512;
    bool m_cubeMapResolutionChanged = false;

    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_sampler;
    Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

    std::function<void(Wolf::ResourceUniqueOwner<Wolf::Image>&)> m_onCubeMapChangedCallback;
};
