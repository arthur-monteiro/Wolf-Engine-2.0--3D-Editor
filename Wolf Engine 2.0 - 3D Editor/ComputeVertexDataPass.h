#pragma once

#include <CommandRecordBase.h>
#include <DescriptorSetLayoutGenerator.h>
#include <ShaderBindingTable.h>
#include <ShaderParser.h>
#include <UniformBuffer.h>

#include "Mesh.h"
#include "ModelLoader.h"
#include "RayTracedWorldManager.h"

namespace Wolf
{
    class Mesh;
}

class ComputeVertexDataPass : public Wolf::CommandRecordBase
{
public:
    ~ComputeVertexDataPass();

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    bool hasCommandsRecordedThisFrame() const { return m_commandsRecordedThisFrame; }

    class Request
    {
    public:
        Request() = default;
        Request(const Wolf::ResourceNonOwner<Wolf::Mesh>& mesh, const Wolf::NullableResourceNonOwner<Wolf::Buffer>& overrideIndexBuffer, uint32_t firstMaterialIdx, const Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure>& blas)
             : m_mesh(mesh), m_firstMaterialIdx(firstMaterialIdx), m_bottomLevelAccelerationStructure(blas)
        {
            if (mesh->getVertexSize() != sizeof(Wolf::Vertex3D))
            {
                Wolf::Debug::sendCriticalError("Vertex must be Wolf::Vertex3D");
            }

            if (overrideIndexBuffer)
            {
                m_indexBuffer = overrideIndexBuffer;
            }
            else
            {
                m_indexBuffer = mesh->getIndexBuffer();
            }

            initResources();
        }
        Request(Request& other)
        {
            m_mesh = other.m_mesh;
            m_indexBuffer = other.m_indexBuffer;
            m_firstMaterialIdx = other.m_firstMaterialIdx;
            m_bottomLevelAccelerationStructure = other.m_bottomLevelAccelerationStructure;

            m_uniformBuffer.reset(other.m_uniformBuffer.release());
            m_colorWeightPerVertex.reset(other.m_colorWeightPerVertex.release());

            m_resetBuffersDescriptorSet.reset(other.m_resetBuffersDescriptorSet.release());

            m_accumulatedNormalsDescriptorSet.reset(other.m_accumulatedNormalsDescriptorSet.release());

            m_accumulateColorsDescriptorSet.reset(other.m_accumulateColorsDescriptorSet.release());
            m_rayTracedWorldManager.reset(other.m_rayTracedWorldManager.release());

            m_averageColorsDescriptorSet.reset(other.m_averageColorsDescriptorSet.release());
        }

        void recordCommands(const Wolf::CommandBuffer* commandBuffer, const Wolf::RecordContext& context);

    private:
        void initResources();
        void createResetBuffersDescriptorSet();
        void createAccumulateNormalsDescriptorSet();
        void createAccumulateColorsDescriptorSet();
        void createAverageColorsDescriptorSet();

        Wolf::ResourceNonOwner<Wolf::Mesh> m_mesh;
        Wolf::ResourceNonOwner<Wolf::Buffer> m_indexBuffer;
        uint32_t m_firstMaterialIdx;
        Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure> m_bottomLevelAccelerationStructure;

        struct UniformBufferData
        {
            uint32_t indexCount;
            uint32_t vertexCount;
            glm::vec2 m_padding;
        };
        Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;
        Wolf::ResourceUniqueOwner<Wolf::Buffer> m_colorWeightPerVertex;

        // Step 0: Reset buffers
        Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_resetBuffersDescriptorSet;

        // Step 1: Accumulate normals
        Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_accumulatedNormalsDescriptorSet;

        // Step 3: Accumulate colors
        Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_accumulateColorsDescriptorSet;
        Wolf::ResourceUniqueOwner<RayTracedWorldManager> m_rayTracedWorldManager;

        // Step 4: Average colors
        Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_averageColorsDescriptorSet;
    };
    void addRequestBeforeFrame(Request& request);

private:
    static constexpr uint32_t RequestsBatchSize = 4;
    static constexpr uint32_t QueuesBatchSize = 2;

    Wolf::DynamicResourceUniqueOwnerArray<Request, RequestsBatchSize> m_addRequestsQueue;
    std::mutex m_addRequestQueueMutex;
    Wolf::DynamicStableArray<Wolf::DynamicResourceUniqueOwnerArray<Request, RequestsBatchSize>, QueuesBatchSize> m_currentRequestsQueues;

    bool m_commandsRecordedThisFrame = false;

    // Step 0: Reset buffers
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_resetBuffersShaderParser;
    static Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_resetBuffersPipeline;
    void createResetBuffersPipeline();

    static Wolf::DescriptorSetLayoutGenerator m_resetBuffersDescriptorSetLayoutGenerator;
    static Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_resetBuffersDescriptorSetLayout;

    // Step 1: Accumulate normals
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_accumulateNormalsShaderParser;
    static Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_accumulateNormalsPipeline;
    void createAccumulateNormalsPipeline();

    static Wolf::DescriptorSetLayoutGenerator m_accumulateNormalsDescriptorSetLayoutGenerator;
    static Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_accumulateNormalsDescriptorSetLayout;

    // Step 2: Average normals
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_averageNormalsShaderParser;
    static Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_averageNormalsPipeline;
    void createAverageNormalsPipeline();

    // Step 3: Accumulate colors
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayGenShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_rayMissShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_closestHitShaderParser;
    static Wolf::ResourceUniqueOwner<Wolf::ShaderBindingTable> m_accumulateColorsShaderBindingTable;
    static Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_accumulateColorsPipeline;
    static Wolf::ResourceUniqueOwner<Wolf::Buffer> m_randomSamplesBuffer;
    void createAccumulateColorsPipeline();

    static Wolf::DescriptorSetLayoutGenerator m_accumulateColorsDescriptorSetLayoutGenerator;
    static Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_accumulateColorsDescriptorSetLayout;

    // Step 4: Average colors
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_averageColorsShaderParser;
    static Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_averageColorsPipeline;
    void createAverageColorsPipeline();

    static Wolf::DescriptorSetLayoutGenerator m_averageColorsDescriptorSetLayoutGenerator;
    static Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_averageColorsDescriptorSetLayout;
};
