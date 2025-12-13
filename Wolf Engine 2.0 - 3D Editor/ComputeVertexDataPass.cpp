#include "ComputeVertexDataPass.h"

#include <Configuration.h>
#include <random>
#include <RayTracingShaderGroupGenerator.h>

#include "DebugMarker.h"
#include "RayTracedWorldManager.h"

Wolf::DescriptorSetLayoutGenerator ComputeVertexDataPass::m_resetBuffersDescriptorSetLayoutGenerator;
Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> ComputeVertexDataPass::m_resetBuffersDescriptorSetLayout;
Wolf::ResourceUniqueOwner<Wolf::Pipeline> ComputeVertexDataPass::m_resetBuffersPipeline;

Wolf::DescriptorSetLayoutGenerator ComputeVertexDataPass::m_accumulateNormalsDescriptorSetLayoutGenerator;
Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> ComputeVertexDataPass::m_accumulateNormalsDescriptorSetLayout;
Wolf::ResourceUniqueOwner<Wolf::Pipeline> ComputeVertexDataPass::m_accumulateNormalsPipeline;

Wolf::ResourceUniqueOwner<Wolf::Pipeline> ComputeVertexDataPass::m_averageNormalsPipeline;

Wolf::DescriptorSetLayoutGenerator ComputeVertexDataPass::m_accumulateColorsDescriptorSetLayoutGenerator;
Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> ComputeVertexDataPass::m_accumulateColorsDescriptorSetLayout;

Wolf::ResourceUniqueOwner<Wolf::Pipeline> ComputeVertexDataPass::m_accumulateColorsPipeline;
Wolf::ResourceUniqueOwner<Wolf::ShaderBindingTable> ComputeVertexDataPass::m_accumulateColorsShaderBindingTable;
Wolf::ResourceUniqueOwner<Wolf::Buffer> ComputeVertexDataPass::m_randomSamplesBuffer;

Wolf::DescriptorSetLayoutGenerator ComputeVertexDataPass::m_averageColorsDescriptorSetLayoutGenerator;
Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> ComputeVertexDataPass::m_averageColorsDescriptorSetLayout;
Wolf::ResourceUniqueOwner<Wolf::Pipeline> ComputeVertexDataPass::m_averageColorsPipeline;

ComputeVertexDataPass::~ComputeVertexDataPass()
{
    m_resetBuffersDescriptorSetLayoutGenerator.reset();
    m_resetBuffersDescriptorSetLayout.reset(nullptr);
    m_resetBuffersPipeline.reset(nullptr);

    m_accumulateNormalsDescriptorSetLayoutGenerator.reset();
    m_accumulateNormalsDescriptorSetLayout.reset(nullptr);
    m_accumulateNormalsPipeline.reset(nullptr);

    m_averageNormalsPipeline.reset(nullptr);

    m_accumulateColorsDescriptorSetLayoutGenerator.reset();
    m_accumulateColorsDescriptorSetLayout.reset(nullptr);
    m_accumulateColorsPipeline.reset(nullptr);
    m_accumulateColorsShaderBindingTable.reset(nullptr);
    m_randomSamplesBuffer.reset(nullptr);

    m_averageColorsDescriptorSetLayoutGenerator.reset();
    m_averageColorsDescriptorSetLayout.reset(nullptr);
    m_averageColorsPipeline.reset(nullptr);
}

void ComputeVertexDataPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::RAY_TRACING, false));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

    m_currentRequestsQueues.resize(Wolf::g_configuration->getMaxCachedFrames());

    // Step 0: Reset buffers
    m_resetBuffersShaderParser.reset(new Wolf::ShaderParser("Shaders/computeVertexData/resetBuffers.comp"));

    m_resetBuffersDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 0); // vertex buffer
    m_resetBuffersDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 1); // index buffer
    m_resetBuffersDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 2); // uniform buffer
    m_resetBuffersDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 3); // color weight per vertex
    m_resetBuffersDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_resetBuffersDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    createResetBuffersPipeline();

    // Step 1: Accumulate normals
    m_accumulateNormalsShaderParser.reset(new Wolf::ShaderParser("Shaders/computeVertexData/accumulateNormals.comp"));

    m_accumulateNormalsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 0); // vertex buffer
    m_accumulateNormalsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 1); // index buffer
    m_accumulateNormalsDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 2); // uniform buffer
    m_accumulateNormalsDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_accumulateNormalsDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    createAccumulateNormalsPipeline();

    // Step 2: Average normals
    m_averageNormalsShaderParser.reset(new Wolf::ShaderParser("Shaders/computeVertexData/averageNormals.comp"));

    createAverageNormalsPipeline();

    // Step 3: Accumulate colors
    Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
    RayTracedWorldManager::addRayGenShaderCode(shaderCodeToAdd, 1);
    m_rayGenShaderParser.reset(new Wolf::ShaderParser("Shaders/computeVertexData/accumulateColors/shader.rgen", {}, -1, -1, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_rayMissShaderParser.reset(new Wolf::ShaderParser("Shaders/computeVertexData/accumulateColors/shader.rmiss", {}, -1, -1, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_closestHitShaderParser.reset(new Wolf::ShaderParser("Shaders/computeVertexData/accumulateColors/shader.rchit", {}, -1, 2, -1,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));

    m_accumulateColorsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 0); // vertex buffer
    m_accumulateColorsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 1); // index buffer
    m_accumulateColorsDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 2); // uniform buffer
    m_accumulateColorsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 3); // random samples
    m_accumulateColorsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 4); // color weight per vertex
    m_accumulateColorsDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_accumulateColorsDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    createAccumulateColorsPipeline();

    constexpr uint32_t RANDOM_SAMPLE_COUNT = 32;
    m_randomSamplesBuffer.reset(Wolf::Buffer::createBuffer(RANDOM_SAMPLE_COUNT * sizeof(glm::vec2), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    std::vector<glm::vec2> randomSamples(RANDOM_SAMPLE_COUNT);

    std::default_random_engine generator;
    std::uniform_real_distribution distrib(0.0f, 1.0f);
    for (uint32_t i = 0; i < RANDOM_SAMPLE_COUNT; ++i)
    {
        randomSamples[i] = glm::vec2(distrib(generator), distrib(generator));
    }

    m_randomSamplesBuffer->transferCPUMemoryWithStagingBuffer(randomSamples.data(), RANDOM_SAMPLE_COUNT * sizeof(glm::vec2));

    // Step 4: Average colors
    m_averageColorsShaderParser.reset(new Wolf::ShaderParser("Shaders/computeVertexData/averageColors.comp"));

    m_averageColorsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 0); // vertex buffer
    m_averageColorsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 1); // index buffer
    m_averageColorsDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 2); // uniform buffer
    m_averageColorsDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::COMPUTE, 3); // color weight per vertex
    m_averageColorsDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_averageColorsDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    createAverageColorsPipeline();
}

void ComputeVertexDataPass::resize(const Wolf::InitializationContext& context)
{
    // Nothing to do
}

void ComputeVertexDataPass::record(const Wolf::RecordContext& context)
{
    int32_t requestQueueIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber() % Wolf::g_configuration->getMaxCachedFrames();

    m_currentRequestsQueues[requestQueueIdx].swap(m_addRequestsQueue);
    m_addRequestsQueue.clear();

    if (m_currentRequestsQueues[requestQueueIdx].empty())
    {
        m_commandsRecordedThisFrame = false;
        return;
    }

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::renderPassDebugColor, "GPU buffer to GPU buffer copy pass");

    for (uint32_t i = 0; i < m_currentRequestsQueues[requestQueueIdx].size(); ++i)
    {
        const Wolf::ResourceUniqueOwner<Request>& request = m_currentRequestsQueues[requestQueueIdx][i];
        request->recordCommands(m_commandBuffer.get(), context);
    }

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_commandsRecordedThisFrame = true;
}

void ComputeVertexDataPass::submit(const Wolf::SubmitContext& context)
{
    if (m_commandsRecordedThisFrame)
    {
        std::vector<const Wolf::Semaphore*> waitSemaphores{ };
        const std::vector<const Wolf::Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
        m_commandBuffer->submit(waitSemaphores, signalSemaphores, nullptr);
    }

    bool anyShaderModified = m_resetBuffersShaderParser->compileIfFileHasBeenModified();

    if (m_accumulateNormalsShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;

    if (m_rayGenShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;
    if (m_rayMissShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;
    if (m_closestHitShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;

    if (m_averageColorsShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;

    if (anyShaderModified)
    {
        context.graphicAPIManager->waitIdle();
        createResetBuffersPipeline();
        createAccumulateNormalsPipeline();
        createAccumulateColorsPipeline();
        createAverageColorsPipeline();
    }
}

void ComputeVertexDataPass::Request::recordCommands(const Wolf::CommandBuffer* commandBuffer, const Wolf::RecordContext& context)
{
    uint32_t indexCount = static_cast<uint32_t>(m_indexBuffer->getSize() / sizeof(uint32_t));
    uint32_t vertexCount = m_mesh->getVertexCount();

    UniformBufferData uniformBufferData{};
    uniformBufferData.indexCount = indexCount;
    uniformBufferData.vertexCount = vertexCount;
    m_uniformBuffer->transferCPUMemory(&uniformBufferData, sizeof(UniformBufferData));

    // Step 0: Reset buffers
    commandBuffer->bindPipeline(m_resetBuffersPipeline.createConstNonOwnerResource());
    commandBuffer->bindDescriptorSet(m_resetBuffersDescriptorSet.createConstNonOwnerResource(), 0, *m_resetBuffersPipeline);

    constexpr Wolf::Extent3D dispatchGroups = { 256, 1, 1 };
    const uint32_t groupSizeX = indexCount % dispatchGroups.width != 0 ? indexCount / dispatchGroups.width + 1 : indexCount / dispatchGroups.width;
    commandBuffer->dispatch(groupSizeX, 1, 1);

    m_mesh->getVertexBuffer()->recordBarrier(commandBuffer, { Wolf::PipelineStage::COMPUTE_SHADER, Wolf::AccessFlagBits::SHADER_WRITE },
        { Wolf::PipelineStage::COMPUTE_SHADER, Wolf::AccessFlagBits::SHADER_WRITE });

    // Step 1: Accumulate normals
    commandBuffer->bindPipeline(m_accumulateNormalsPipeline.createConstNonOwnerResource());
    commandBuffer->bindDescriptorSet(m_accumulatedNormalsDescriptorSet.createConstNonOwnerResource(), 0, *m_accumulateNormalsPipeline);

    commandBuffer->dispatch(groupSizeX, 1, 1);

    m_mesh->getVertexBuffer()->recordBarrier(commandBuffer, { Wolf::PipelineStage::COMPUTE_SHADER, Wolf::AccessFlagBits::SHADER_WRITE },
        { Wolf::PipelineStage::COMPUTE_SHADER, Wolf::AccessFlagBits::SHADER_WRITE });

    // Step 2: Average normals
    commandBuffer->bindPipeline(m_averageNormalsPipeline.createConstNonOwnerResource());
    commandBuffer->bindDescriptorSet(m_accumulatedNormalsDescriptorSet.createConstNonOwnerResource(), 0, *m_accumulateNormalsPipeline);

    const uint32_t allVerticesGroupSizeX = vertexCount % dispatchGroups.width != 0 ? vertexCount / dispatchGroups.width + 1 : vertexCount / dispatchGroups.width;
    commandBuffer->dispatch(allVerticesGroupSizeX, 1, 1);

    m_mesh->getVertexBuffer()->recordBarrier(commandBuffer, { Wolf::PipelineStage::COMPUTE_SHADER, Wolf::AccessFlagBits::SHADER_WRITE },
        { Wolf::PipelineStage::RAY_TRACING_SHADER, Wolf::AccessFlagBits::SHADER_WRITE });

    // Step 3: Accumulate colors
    m_rayTracedWorldManager->build(*commandBuffer);
    m_rayTracedWorldManager->recordTLASBuildBarriers(*commandBuffer);

    commandBuffer->bindPipeline(m_accumulateColorsPipeline.createConstNonOwnerResource());
    commandBuffer->bindDescriptorSet(m_accumulateColorsDescriptorSet.createConstNonOwnerResource(), 0, *m_accumulateColorsPipeline);
    commandBuffer->bindDescriptorSet(m_rayTracedWorldManager->getDescriptorSet(), 1, *m_accumulateColorsPipeline);
    commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 2, *m_accumulateColorsPipeline);

    commandBuffer->traceRays(m_accumulateColorsShaderBindingTable.createConstNonOwnerResource(), { indexCount, 1, 1 });

    m_mesh->getVertexBuffer()->recordBarrier(commandBuffer, { Wolf::PipelineStage::RAY_TRACING_SHADER, Wolf::AccessFlagBits::SHADER_WRITE },
        { Wolf::PipelineStage::COMPUTE_SHADER, Wolf::AccessFlagBits::SHADER_WRITE });
    m_colorWeightPerVertex->recordBarrier(commandBuffer, { Wolf::PipelineStage::RAY_TRACING_SHADER, Wolf::AccessFlagBits::SHADER_WRITE },
        { Wolf::PipelineStage::COMPUTE_SHADER, Wolf::AccessFlagBits::SHADER_READ });

    // Step 4: Average colors
    commandBuffer->bindPipeline(m_averageColorsPipeline.createConstNonOwnerResource());
    commandBuffer->bindDescriptorSet(m_averageColorsDescriptorSet.createConstNonOwnerResource(), 0, *m_averageColorsPipeline);
    commandBuffer->dispatch(allVerticesGroupSizeX, 1, 1);
}

void ComputeVertexDataPass::Request::initResources()
{
    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));

    uint32_t colorWeightPerVertexBufferSize = m_mesh->getVertexCount() * sizeof(float);
    m_colorWeightPerVertex.reset(Wolf::Buffer::createBuffer(colorWeightPerVertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    // Step 0: Reset buffers
    createResetBuffersDescriptorSet();

    // Step 1: Accumulate normals
    createAccumulateNormalsDescriptorSet();

    // Step 3: Accumulate colors
    m_rayTracedWorldManager.reset(new RayTracedWorldManager());

    RayTracedWorldManager::RayTracedWorldInfo rayTracedWorldInfo{};
    rayTracedWorldInfo.m_instances.resize(1);
    rayTracedWorldInfo.m_instances[0].m_bottomLevelAccelerationStructure = m_bottomLevelAccelerationStructure;
    rayTracedWorldInfo.m_instances[0].m_transform = glm::mat4(1.0f);
    rayTracedWorldInfo.m_instances[0].m_firstMaterialIdx = m_firstMaterialIdx;
    rayTracedWorldInfo.m_instances[0].m_mesh = m_mesh;
    m_rayTracedWorldManager->requestBuild(rayTracedWorldInfo);

    createAccumulateColorsDescriptorSet();

    // Step 4: Average colors
    createAverageColorsDescriptorSet();
}

void ComputeVertexDataPass::Request::createResetBuffersDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_resetBuffersDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setBuffer(0, *m_mesh->getVertexBuffer());
    descriptorSetGenerator.setBuffer(1, *m_indexBuffer);
    descriptorSetGenerator.setUniformBuffer(2, *m_uniformBuffer);
    descriptorSetGenerator.setBuffer(3, *m_colorWeightPerVertex);

    if (!m_resetBuffersDescriptorSet)
        m_resetBuffersDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_resetBuffersDescriptorSetLayout));
    m_resetBuffersDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void ComputeVertexDataPass::Request::createAccumulateNormalsDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_accumulateNormalsDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setBuffer(0, *m_mesh->getVertexBuffer());
    descriptorSetGenerator.setBuffer(1, *m_indexBuffer);
    descriptorSetGenerator.setUniformBuffer(2, *m_uniformBuffer);

    if (!m_accumulatedNormalsDescriptorSet)
        m_accumulatedNormalsDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_accumulateNormalsDescriptorSetLayout));
    m_accumulatedNormalsDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void ComputeVertexDataPass::Request::createAccumulateColorsDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_accumulateColorsDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setBuffer(0, *m_mesh->getVertexBuffer());
    descriptorSetGenerator.setBuffer(1, *m_indexBuffer);
    descriptorSetGenerator.setUniformBuffer(2, *m_uniformBuffer);
    descriptorSetGenerator.setBuffer(3, *m_randomSamplesBuffer);
    descriptorSetGenerator.setBuffer(4, *m_colorWeightPerVertex);

    if (!m_accumulateColorsDescriptorSet)
        m_accumulateColorsDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_accumulateColorsDescriptorSetLayout));
    m_accumulateColorsDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void ComputeVertexDataPass::Request::createAverageColorsDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_averageColorsDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setBuffer(0, *m_mesh->getVertexBuffer());
    descriptorSetGenerator.setBuffer(1, *m_indexBuffer);
    descriptorSetGenerator.setUniformBuffer(2, *m_uniformBuffer);
    descriptorSetGenerator.setBuffer(3, *m_colorWeightPerVertex);

    if (!m_averageColorsDescriptorSet)
        m_averageColorsDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_averageColorsDescriptorSetLayout));
    m_averageColorsDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void ComputeVertexDataPass::addRequestBeforeFrame(Request& request)
{
    m_addRequestQueueMutex.lock();
    m_addRequestsQueue.emplace_back(new Request(request));
    m_addRequestQueueMutex.unlock();
}

void ComputeVertexDataPass::createResetBuffersPipeline()
{
    Wolf::ShaderCreateInfo computeShaderCreateInfo{};
    computeShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;
    m_resetBuffersShaderParser->readCompiledShader(computeShaderCreateInfo.shaderCode);

    std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts =
    {
        m_resetBuffersDescriptorSetLayout.createConstNonOwnerResource()
    };
    m_resetBuffersPipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderCreateInfo, descriptorSetLayouts));
}

void ComputeVertexDataPass::createAccumulateNormalsPipeline()
{
    Wolf::ShaderCreateInfo computeShaderCreateInfo{};
    computeShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;
    m_accumulateNormalsShaderParser->readCompiledShader(computeShaderCreateInfo.shaderCode);

    std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts =
    {
        m_accumulateNormalsDescriptorSetLayout.createConstNonOwnerResource()
    };
    m_accumulateNormalsPipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderCreateInfo, descriptorSetLayouts));
}

void ComputeVertexDataPass::createAverageNormalsPipeline()
{
    Wolf::ShaderCreateInfo computeShaderCreateInfo{};
    computeShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;
    m_averageNormalsShaderParser->readCompiledShader(computeShaderCreateInfo.shaderCode);

    std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts =
    {
        m_accumulateNormalsDescriptorSetLayout.createConstNonOwnerResource()
    };
    m_averageNormalsPipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderCreateInfo, descriptorSetLayouts));
}

void ComputeVertexDataPass::createAccumulateColorsPipeline()
{
    Wolf::RayTracingShaderGroupGenerator shaderGroupGenerator;
    shaderGroupGenerator.addRayGenShaderStage(0);
    shaderGroupGenerator.addMissShaderStage(1);
    Wolf::HitGroup hitGroup;
    hitGroup.closestHitShaderIdx = 2;
    shaderGroupGenerator.addHitGroup(hitGroup);

    Wolf::RayTracingPipelineCreateInfo pipelineCreateInfo;

    std::vector<char> rayGenShaderCode;
    m_rayGenShaderParser->readCompiledShader(rayGenShaderCode);
    std::vector<char> rayMissShaderCode;
    m_rayMissShaderParser->readCompiledShader(rayMissShaderCode);
    std::vector<char> closestHitShaderCode;
    m_closestHitShaderParser->readCompiledShader(closestHitShaderCode);

    std::vector<Wolf::ShaderCreateInfo> shaders(3);
    shaders[0].shaderCode = rayGenShaderCode;
    shaders[0].stage = Wolf::ShaderStageFlagBits::RAYGEN;
    shaders[1].shaderCode = rayMissShaderCode;
    shaders[1].stage = Wolf::ShaderStageFlagBits::MISS;
    shaders[2].shaderCode = closestHitShaderCode;
    shaders[2].stage = Wolf::ShaderStageFlagBits::CLOSEST_HIT;
    pipelineCreateInfo.shaderCreateInfos = shaders;

    pipelineCreateInfo.shaderGroupsCreateInfos = shaderGroupGenerator.getShaderGroups();

    std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts =
    {
        m_accumulateColorsDescriptorSetLayout.createConstNonOwnerResource(),
        RayTracedWorldManager::getDescriptorSetLayout().createConstNonOwnerResource(),
        Wolf::MaterialsGPUManager::getDescriptorSetLayout().createConstNonOwnerResource()
    };
    m_accumulateColorsPipeline.reset(Wolf::Pipeline::createRayTracingPipeline(pipelineCreateInfo, descriptorSetLayouts));

    m_accumulateColorsShaderBindingTable.reset(new Wolf::ShaderBindingTable(static_cast<uint32_t>(shaders.size()), *m_accumulateColorsPipeline));
}

void ComputeVertexDataPass::createAverageColorsPipeline()
{
    Wolf::ShaderCreateInfo computeShaderCreateInfo{};
    computeShaderCreateInfo.stage = Wolf::ShaderStageFlagBits::COMPUTE;
    m_averageColorsShaderParser->readCompiledShader(computeShaderCreateInfo.shaderCode);

    std::vector<Wolf::ResourceReference<const Wolf::DescriptorSetLayout>> descriptorSetLayouts =
    {
        m_averageColorsDescriptorSetLayout.createConstNonOwnerResource()
    };
    m_averageColorsPipeline.reset(Wolf::Pipeline::createComputePipeline(computeShaderCreateInfo, descriptorSetLayouts));
}
