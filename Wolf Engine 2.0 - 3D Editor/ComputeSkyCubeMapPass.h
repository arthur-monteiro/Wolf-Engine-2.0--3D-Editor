#pragma once

#include <glm/glm.hpp>

#include <CommandRecordBase.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <FrameBuffer.h>
#include <Mesh.h>
#include <ResourceUniqueOwner.h>
#include <RenderPass.h>
#include <Sampler.h>
#include <ShaderParser.h>
#include <UniformBuffer.h>

#include "SkyBoxManager.h"

class ComputeSkyCubeMapPass : public Wolf::CommandRecordBase
{
public:
    explicit ComputeSkyCubeMapPass(const Wolf::ResourceNonOwner<SkyBoxManager>& skyboxManager) :  m_skyBoxManager(skyboxManager) {}

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    [[nodiscard]] bool wasEnabledThisFrame() const { return m_drawRecordedThisFrame; }
    void setInputSphericalMap(const Wolf::ResourceNonOwner<Wolf::Image>& sphericalMap);

    void onCubeMapChanged();

private:
    Wolf::ResourceNonOwner<SkyBoxManager> m_skyBoxManager;

    void createRenderTarget();
    void createRenderPass();

    void updateComputeFromSphericalMapDescriptorSets();
    void createComputeFromSphericalMapPipeline();

    Wolf::ResourceUniqueOwner<Wolf::RenderPass> m_renderPass;
    Wolf::ResourceUniqueOwner<Wolf::FrameBuffer> m_frameBuffer;

    Wolf::ResourceUniqueOwner<Wolf::Image> m_renderTargetImage;

    bool m_drawRecordedThisFrame = false;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_vertexShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_fragmentShaderParser;

    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_computeFromSphericalMapPipeline;

    struct ComputeFromSphericalMapUniformBufferData
    {
        glm::mat4 viewProjectionMatrix;
    };
    std::array<Wolf::ResourceUniqueOwner<Wolf::UniformBuffer>, 6> m_computeFromSphericalMapUniformBuffers;
    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_computeFromSphericalMapSampler;
    Wolf::DescriptorSetLayoutGenerator m_computeFromSphericalMapDescriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_computeFromSphericalMapDescriptorSetLayout;
    std::array<Wolf::ResourceUniqueOwner<Wolf::DescriptorSet>, 6> m_computeFromSphericalMapDescriptorSets;
    bool m_updateComputeFromSphericalMapDescriptorSetRequested = false;

    Wolf::NullableResourceNonOwner<Wolf::Image> m_sphericalMapImage;
};
