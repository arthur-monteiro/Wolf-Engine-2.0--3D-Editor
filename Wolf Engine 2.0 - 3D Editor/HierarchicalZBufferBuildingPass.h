#pragma once

#include <CommandRecordBase.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Mesh.h>
#include <Pipeline.h>
#include <ResourceUniqueOwner.h>
#include <Sampler.h>
#include <ShaderParser.h>

#include "ForwardPass.h"
#include "PreDepthPass.h"

class HierarchicalZBufferBuildingPass : public Wolf::CommandRecordBase
{
public:
    HierarchicalZBufferBuildingPass(const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<ForwardPass>& forwardPass, EditorParams* editorParams);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    [[nodiscard]] Wolf::ResourceNonOwner<Wolf::Image> getOutputImage() const { return m_outputImage.createNonOwnerResource(); }

private:
    void createOutputImage();
    void updateDescriptorSet();
    void createPipeline();

    Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
    Wolf::ResourceNonOwner<ForwardPass> m_forwardPass;
    EditorParams* m_editorParams;

    Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
    Wolf::Extent3D m_extent;
    uint32_t m_mipCount;
    static constexpr uint32_t MAX_HZB_MIP_COUNT = 12;

    Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;
    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_sampler;
    Wolf::ResourceUniqueOwner<Wolf::Buffer> m_counterBuffer;

    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_computeShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_pipeline;

    struct PushConstants
    {
        uint32_t m_srcWidth;
        uint32_t m_srcHeight;
        uint32_t m_numMips;
        uint32_t m_workgroupSlice;

        glm::vec4 m_viewport;
    };
};
