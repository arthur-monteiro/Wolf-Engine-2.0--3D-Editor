#pragma once

#include <glm/glm.hpp>

#include <CommandRecordBase.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Pipeline.h>
#include <ResourceUniqueOwner.h>
#include <ShaderParser.h>
#include <UniformBuffer.h>

#include "CustomSceneRenderPass.h"

class SurfaceCoatingEmitterComponent;

class SurfaceCoatingDataPreparationPass : public Wolf::CommandRecordBase
{
public:
    SurfaceCoatingDataPreparationPass(const Wolf::ResourceNonOwner<CustomSceneRenderPass>& customSceneRenderPass);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    void registerComponent(SurfaceCoatingEmitterComponent* component);
    void unregisterComponent(const SurfaceCoatingEmitterComponent* component);

private:
    Wolf::ResourceNonOwner<CustomSceneRenderPass> m_customSceneRenderPass;

    void createPipeline();
    void createOrUpdateDescriptorSet();

    bool m_commandsRecordedThisFrame = false;
    SurfaceCoatingEmitterComponent* m_component = nullptr;

    Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_computeShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_pipeline;

    // Per request
    struct UniformBufferData
    {
        glm::mat4 transform;

        glm::mat4 depthViewProjMatrix;

        glm::vec2 depthImageSize;
        glm::vec2 padding;
    };
    Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;
};
