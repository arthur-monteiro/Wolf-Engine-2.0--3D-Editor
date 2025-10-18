#pragma once

#include <Attachment.h>
#include <CommandRecordBase.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <FrameBuffer.h>
#include <Mesh.h>
#include <RenderPass.h>
#include <ResourceUniqueOwner.h>
#include <Sampler.h>
#include <ShaderParser.h>

#include "ForwardPass.h"

class CompositionPass : public Wolf::CommandRecordBase
{
public:
    CompositionPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<ForwardPass>& forwardPass);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    void clear();

    void setInputLUT(const Wolf::ResourceNonOwner<Wolf::Image>& lutImage);
    void releaseInputLUT();

    void saveSwapChainToFile();

private:
    void updateDescriptorSets();
    void createPipeline();

    EditorParams* m_editorParams;
    Wolf::ResourceNonOwner<ForwardPass> m_forwardPass;

    Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
    std::vector<Wolf::ResourceUniqueOwner<Wolf::DescriptorSet>> m_descriptorSets;
    Wolf::ResourceUniqueOwner<Wolf::Mesh> m_fullscreenRect;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_computeShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_pipeline;

    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_sampler;
    bool m_updateDescriptorSetRequested = false;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_defaultLUTImage;
    Wolf::NullableResourceNonOwner<Wolf::Image> m_lutImage;

    struct UBData
    {
        uint32_t displayType;
        float lutTexelSize;
        uint32_t renderOffsetX;
        uint32_t renderOffsetY;

        uint32_t renderSizeX;
        uint32_t renderSizeY;
    };
    std::unique_ptr<Wolf::UniformBuffer> m_uniformBuffer;

    /* Cached resources */
    Wolf::Image* m_lastSwapChainImage = nullptr;
    std::vector<Wolf::Image*> m_swapChainImages;
    Wolf::Image* m_uiImage;
};
