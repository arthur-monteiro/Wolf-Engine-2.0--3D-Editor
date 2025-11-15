#include "VoxelGlobalIlluminationPass.h"

#include <DebugMarker.h>
#include <DescriptorSetGenerator.h>
#include <fstream>
#include <GraphicCameraInterface.h>
#include <random>
#include <RayTracingShaderGroupGenerator.h>
#include <ShaderBindingTable.h>
#include <ShaderParser.h>

#include "CommonLayouts.h"

VoxelGlobalIlluminationPass::VoxelGlobalIlluminationPass(const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager)
: m_rayTracedWorldManager(rayTracedWorldManager)
{
}

void VoxelGlobalIlluminationPass::setResourceManager(const Wolf::ResourceNonOwner<ResourceManager>& resourceManager)
{
    m_resourceManager = resourceManager;
    m_sphereMeshResourceId = m_resourceManager->addModel("Models/sphere.obj");
}

void VoxelGlobalIlluminationPass::addMeshesToRenderList(const Wolf::ResourceNonOwner<Wolf::RenderMeshList>& renderMeshList)
{
    if (m_enableDebug)
    {
        // ReSharper disable once CppDFAUnreachableCode
        if (m_sphereMeshResourceId == ResourceManager::NO_RESOURCE || !m_resourceManager->isModelLoaded(m_sphereMeshResourceId))
            return;

        Wolf::ModelData* modelData = m_resourceManager->getModelData(m_sphereMeshResourceId);

        Wolf::RenderMeshList::InstancedMesh instancedMesh{};
        instancedMesh.mesh.mesh = modelData->mesh.createNonOwnerResource();
        instancedMesh.mesh.pipelineSet = m_debugPipelineSet.createConstNonOwnerResource();
        instancedMesh.mesh.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].push_back(Wolf::DescriptorSetBindInfo(m_debugDescriptorSet.createConstNonOwnerResource(),
            m_debugDescriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_MESH_DEBUG));

        instancedMesh.instanceBuffer = m_sphereInstanceBuffer.createNonOwnerResource();
        instancedMesh.instanceSize = sizeof(SphereInstanceData);

        renderMeshList->addTransientInstancedMesh(instancedMesh, GRID_SIZE * GRID_SIZE * GRID_SIZE);
    }
}

void VoxelGlobalIlluminationPass::initializeResources(const Wolf::InitializationContext& context)
{
    m_commandBuffer.reset(Wolf::CommandBuffer::createCommandBuffer(Wolf::QueueType::RAY_TRACING, false /* isTransient */));
    createSemaphores(context, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false);

    initGridUpdateShaders();

    m_rayTracingDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 0); // voxel grid
    m_rayTracingDescriptorSetLayoutGenerator.addUniformBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 1); // uniform buffer
    m_rayTracingDescriptorSetLayoutGenerator.addCombinedImageSampler(Wolf::ShaderStageFlagBits::RAYGEN, 2); // noise map
    m_rayTracingDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::RAYGEN, 3); // requests buffer
    m_rayTracingDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    createVoxelGrid();
    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));
    createNoiseImage();
    createPipeline();
    createDescriptorSet();

    initDebugResources();

    // Output resources
    m_outputDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 0); // voxel grid
    m_outputDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 1); // requests buffer
    m_outputDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_outputDescriptorSetLayoutGenerator.getDescriptorLayouts()));
    createOutputDescriptorSet();
}

void VoxelGlobalIlluminationPass::resize(const Wolf::InitializationContext& context)
{
    // Nothing to do
}

void VoxelGlobalIlluminationPass::record(const Wolf::RecordContext& context)
{
    if (!m_rayTracedWorldManager->hasInstance())
    {
        m_wasEnabledThisFrame = false;
        return;
    }

    UniformBufferData uniformBufferData{};
    uniformBufferData.frameIdx = context.currentFrameIdx;
    m_uniformBuffer->transferCPUMemory(&uniformBufferData, sizeof(UniformBufferData));

    m_commandBuffer->beginCommandBuffer();

    Wolf::DebugMarker::beginRegion(m_commandBuffer.get(), Wolf::DebugMarker::rayTracePassDebugColor, "Path tracing Pass");

    m_commandBuffer->bindPipeline(m_pipeline.createConstNonOwnerResource());
    m_commandBuffer->bindDescriptorSet(m_rayTracingDescriptorSet.createConstNonOwnerResource(), 0, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(m_rayTracedWorldManager->getDescriptorSet(), 1, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(context.bindlessDescriptorSet, 2, *m_pipeline);
    m_commandBuffer->bindDescriptorSet(context.lightManager->getDescriptorSet().createConstNonOwnerResource(), 3, *m_pipeline);

    m_commandBuffer->traceRays(m_shaderBindingTable.createConstNonOwnerResource(), { GRID_SIZE, GRID_SIZE, GRID_SIZE });

    Wolf::DebugMarker::endRegion(m_commandBuffer.get());

    m_commandBuffer->endCommandBuffer();

    m_wasEnabledThisFrame = true;
}

void VoxelGlobalIlluminationPass::submit(const Wolf::SubmitContext& context)
{
    if (!m_wasEnabledThisFrame)
        return;

    std::vector<const Wolf::Semaphore*> waitSemaphores{ };
    const std::vector<const Wolf::Semaphore*> signalSemaphores{ Wolf::CommandRecordBase::getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, VK_NULL_HANDLE);

    bool anyShaderModified = m_rayGenShaderParser->compileIfFileHasBeenModified();
    if (m_rayMissShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;
    if (m_closestHitShaderParser->compileIfFileHasBeenModified())
        anyShaderModified = true;

    if (anyShaderModified)
    {
        context.graphicAPIManager->waitIdle();
        createPipeline();
    }
}

void VoxelGlobalIlluminationPass::addShaderCode(Wolf::ShaderParser::ShaderCodeToAdd& inOutShaderCodeToAdd, uint32_t bindingSlot) const
{
    std::ifstream inFile("Shaders/voxelGI/output/readGlobalIrradiance.glsl");
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

void VoxelGlobalIlluminationPass::createVoxelGrid()
{
    uint32_t voxelCount = GRID_SIZE * GRID_SIZE * GRID_SIZE;
    uint64_t bufferSize = voxelCount * sizeof(VoxelLayout);
    m_voxelGrid.reset(Wolf::Buffer::createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    bufferSize = (6 /* value count per voxel */ * voxelCount) / 32 /* bit count per value (uint32) */ + 1 /* add one value */;
    m_requestsBuffer.reset(Wolf::Buffer::createBuffer(bufferSize * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
}

void VoxelGlobalIlluminationPass::initGridUpdateShaders()
{
    Wolf::ShaderParser::ShaderCodeToAdd shaderCodeToAdd;
    m_rayTracedWorldManager->addRayGenShaderCode(shaderCodeToAdd, 1);
    m_rayGenShaderParser.reset(new Wolf::ShaderParser("Shaders/voxelGI/gridUpdate/shader.rgen", {}, -1, 2, 3,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_rayMissShaderParser.reset(new Wolf::ShaderParser("Shaders/voxelGI/gridUpdate/shader.rmiss", {}, -1, -1, 3,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
    m_closestHitShaderParser.reset(new Wolf::ShaderParser("Shaders/voxelGI/gridUpdate/shader.rchit", {}, -1, 2, 3,
        Wolf::ShaderParser::MaterialFetchProcedure(), shaderCodeToAdd));
}

void VoxelGlobalIlluminationPass::createPipeline()
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
        m_rayTracingDescriptorSetLayout.createConstNonOwnerResource(),
        RayTracedWorldManager::getDescriptorSetLayout().createConstNonOwnerResource(),
        Wolf::MaterialsGPUManager::getDescriptorSetLayout().createConstNonOwnerResource(),
        Wolf::LightManager::getDescriptorSetLayout().createConstNonOwnerResource()
    };
    m_pipeline.reset(Wolf::Pipeline::createRayTracingPipeline(pipelineCreateInfo, descriptorSetLayouts));

    m_shaderBindingTable.reset(new Wolf::ShaderBindingTable(static_cast<uint32_t>(shaders.size()), *m_pipeline));
}

void VoxelGlobalIlluminationPass::createDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_rayTracingDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setBuffer(0, *m_voxelGrid);
    descriptorSetGenerator.setUniformBuffer(1, *m_uniformBuffer);
    descriptorSetGenerator.setCombinedImageSampler(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_noiseImage->getDefaultImageView(), *m_noiseSampler);
    descriptorSetGenerator.setBuffer(3, *m_requestsBuffer);

    if (!m_rayTracingDescriptorSet)
        m_rayTracingDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_rayTracingDescriptorSetLayout));
    m_rayTracingDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

float jitter2()
{
    static std::default_random_engine generator(0);
    static std::uniform_real_distribution distrib(0.0f, 1.0f);
    return distrib(generator);
}

glm::vec3 uniformSampleHemisphere2(float r1, float r2)
{
    // cos(theta) = r1 = y
    // cos^2(theta) + sin^2(theta) = 1 -> sin(theta) = sqrtf(1 - cos^2(theta))
    float sinTheta = sqrtf(1 - r1 * r1);
    float phi = 2 * glm::pi<float>() * r2;
    float x = sinTheta * cosf(phi);
    float z = sinTheta * sinf(phi);
    return {x, r1, z};
}

void VoxelGlobalIlluminationPass::createNoiseImage()
{
    Wolf::CreateImageInfo noiseImageCreateInfo;
    noiseImageCreateInfo.extent = { NOISE_TEXTURE_SIZE_PER_SIDE, NOISE_TEXTURE_SIZE_PER_SIDE, NOISE_TEXTURE_VECTOR_COUNT };
    noiseImageCreateInfo.format = Wolf::Format::R32G32B32A32_SFLOAT;
    noiseImageCreateInfo.mipLevelCount = 1;
    noiseImageCreateInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
    if (!m_noiseImage)
    {
        m_noiseImage.reset(Wolf::Image::createImage(noiseImageCreateInfo));
    }

    std::vector<float> noiseData(NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_VECTOR_COUNT * 4);
    for (int texY = 0; texY < NOISE_TEXTURE_SIZE_PER_SIDE; ++texY)
    {
        for (int texX = 0; texX < NOISE_TEXTURE_SIZE_PER_SIDE; ++texX)
        {
            for (int i = 0; i < NOISE_TEXTURE_VECTOR_COUNT; ++i)
            {
                float r1 = jitter2();
                float r2 = jitter2();

                glm::vec3 offsetDirection = uniformSampleHemisphere2(r1, r2);
                offsetDirection = glm::normalize(offsetDirection);

                const uint32_t idx = texX + texY * NOISE_TEXTURE_SIZE_PER_SIDE + i * (NOISE_TEXTURE_SIZE_PER_SIDE * NOISE_TEXTURE_SIZE_PER_SIDE);

                noiseData[4 * idx    ] = offsetDirection.x;
                noiseData[4 * idx + 1] = offsetDirection.y;
                noiseData[4 * idx + 2] = offsetDirection.z;
            }
        }
    }
    m_noiseImage->copyCPUBuffer(reinterpret_cast<unsigned char*>(noiseData.data()), Wolf::Image::SampledInFragmentShader());

    if (!m_noiseSampler)
    {
        m_noiseSampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1.0f, VK_FILTER_NEAREST));
    }
}

void VoxelGlobalIlluminationPass::initDebugResources()
{
    std::vector<SphereInstanceData> spheres;
    spheres.reserve(GRID_SIZE * GRID_SIZE * GRID_SIZE);
    for (uint32_t zIdx = 0; zIdx < GRID_SIZE; ++zIdx)
    {
        for (uint32_t yIdx = 0; yIdx < GRID_SIZE; ++yIdx)
        {
            for (uint32_t xIdx = 0; xIdx < GRID_SIZE; ++xIdx)
            {
                const glm::vec3 probePos = FIRST_PROBE_POS + SPACE_BETWEEN_PROBES * glm::vec3(xIdx, yIdx, zIdx);
                spheres.push_back({ probePos });
            }
        }
    }

    m_sphereInstanceBuffer.reset(Wolf::Buffer::createBuffer(spheres.size() * sizeof(SphereInstanceData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    m_sphereInstanceBuffer->transferCPUMemoryWithStagingBuffer(spheres.data(), spheres.size() * sizeof(SphereInstanceData));

    m_debugDescriptorSetLayoutGenerator.addStorageBuffer(Wolf::ShaderStageFlagBits::FRAGMENT, 0); // voxel grid
    m_debugDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_debugDescriptorSetLayoutGenerator.getDescriptorLayouts()));

    initializeDebugPipelineSet();

    m_debugDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_debugDescriptorSetLayout));
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_debugDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setBuffer(0, *m_voxelGrid);
    m_debugDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void VoxelGlobalIlluminationPass::initializeDebugPipelineSet()
{
    m_debugPipelineSet.reset(new Wolf::PipelineSet);
    Wolf::PipelineSet::PipelineInfo pipelineInfo{};

    /* PreDepth */
    pipelineInfo.shaderInfos.resize(1);
    pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/voxelGI/debug/debug.vert";
    pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;

    // IA
    Wolf::Vertex3D::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);
    SphereInstanceData::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 1, static_cast<uint32_t>(pipelineInfo.vertexInputAttributeDescriptions.size()));

    pipelineInfo.vertexInputBindingDescriptions.resize(2);
    Wolf::Vertex3D::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);
    SphereInstanceData::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[1], 1);

    // Resources
    pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

    // Color Blend
    pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

    pipelineInfo.cullMode = VK_CULL_MODE_NONE;

    // Dynamic states
    pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

    m_debugPipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH);

    /* Shadow maps */
    m_debugPipelineSet->addEmptyPipeline(CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);

    /* Forward */
    pipelineInfo.shaderInfos.resize(2);
    pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/voxelGI/debug/debug.frag";
    pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

    // Dynamic states
    pipelineInfo.enableDepthWrite = false;
    pipelineInfo.depthCompareOp = Wolf::CompareOp::EQUAL;

    m_debugPipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);
}

void VoxelGlobalIlluminationPass::createOutputDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_outputDescriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setBuffer(0, *m_voxelGrid);
    descriptorSetGenerator.setBuffer(1, *m_requestsBuffer);

    if (!m_outputDescriptorSet)
        m_outputDescriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_outputDescriptorSetLayout));
    m_outputDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}
