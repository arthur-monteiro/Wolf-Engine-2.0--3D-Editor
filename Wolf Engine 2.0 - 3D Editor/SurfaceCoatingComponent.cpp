#include "SurfaceCoatingComponent.h"

#include <random>

#include <Pipeline.h>

#include "CommonLayouts.h"
#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"
#include "MaterialComponent.h"
#include "SurfaceCoatingDataPreparationPass.h"

SurfaceCoatingComponent::SurfaceCoatingComponent(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<AssetManager>& resourceManager,
                                                 std::function<void(ComponentInterface*)> requestReloadCallback, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
: m_renderingPipeline(renderingPipeline), m_customRenderPass(renderingPipeline->getCustomRenderPass()), m_resourceManager(resourceManager), m_requestReloadCallback(requestReloadCallback), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
    Wolf::ShaderStageFlags resourcesAccessibility = Wolf::ShaderStageFlagBits::TESSELLATION_CONTROL | Wolf::ShaderStageFlagBits::TESSELLATION_EVALUATION | Wolf::ShaderStageFlagBits::GEOMETRY;
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, resourcesAccessibility, 0, 1); // Depth texture
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, resourcesAccessibility, 1, 1); // Normal texture
    m_descriptorSetLayoutGenerator.addSampler(resourcesAccessibility, 2); // Sampler
    m_descriptorSetLayoutGenerator.addUniformBuffer(resourcesAccessibility, 3); // Uniform buffer
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, resourcesAccessibility, 4, MAX_PATTERNS * 2,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT); // Pattern images
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::STORAGE_IMAGE, resourcesAccessibility, 5, 1); // Pattern index image
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::STORAGE_IMAGE, resourcesAccessibility, 6, 1); // Min / max depth image
    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT));

    m_sampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR));
    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));

    createDefaultPatternImage();
    createPatternIdxImage();

    createSurfaceCoatingSpecificImages();

    m_customRenderResolution = 2048;
    createCustomRenderImages();

    m_rangeAABBMin = glm::vec3(-10.0f);
    m_rangeAABBMax = glm::vec3(10.0f);
    createDepthCamera();

    m_globalThickness = 0.0f;

    m_defaultPipelineSet.reset(new Wolf::LazyInitSharedResource<Wolf::PipelineSet, SurfaceCoatingComponent>([](Wolf::ResourceUniqueOwner<Wolf::PipelineSet>& pipelineSet)
    {
        pipelineSet.reset(new Wolf::PipelineSet);

			Wolf::PipelineSet::PipelineInfo pipelineInfo;

			/* Pre Depth */
			pipelineInfo.shaderInfos.resize(4);
			pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/surfaceCoating/shader.vert";
			pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;

            pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/surfaceCoating/shader.tesc";
            pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::TESSELLATION_CONTROL;
            pipelineInfo.shaderInfos[1].conditionBlocksToInclude.emplace_back("NO_LIGHTING");

            pipelineInfo.shaderInfos[2].shaderFilename = "Shaders/surfaceCoating/shader.tese";
            pipelineInfo.shaderInfos[2].stage = Wolf::ShaderStageFlagBits::TESSELLATION_EVALUATION;
            pipelineInfo.shaderInfos[2].conditionBlocksToInclude.emplace_back("NO_LIGHTING");

            pipelineInfo.shaderInfos[3].shaderFilename = "Shaders/surfaceCoating/shader.geom";
            pipelineInfo.shaderInfos[3].stage = Wolf::ShaderStageFlagBits::GEOMETRY;
            pipelineInfo.shaderInfos[3].conditionBlocksToInclude.emplace_back("NO_LIGHTING");

			// IA
			InstanceData::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

			pipelineInfo.vertexInputBindingDescriptions.resize(1);
			InstanceData::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

            pipelineInfo.topology = Wolf::PrimitiveTopology::PATCH_LIST;

            pipelineInfo.patchControlPoint = 4;

			// Resources
			pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

			// Color Blend
			pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(Wolf::DynamicState::VIEWPORT);

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH);

			/* Shadow maps */
			pipelineInfo.dynamicStates.clear();
			pipelineInfo.depthBiasConstantFactor = 4.0f;
			pipelineInfo.depthBiasSlopeFactor = 2.5f;
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
			pipelineInfo.depthBiasConstantFactor = 0.0f;
			pipelineInfo.depthBiasSlopeFactor = 0.0f;

			/* Forward */
            pipelineInfo.shaderInfos[1].conditionBlocksToInclude.clear();
            pipelineInfo.shaderInfos[2].conditionBlocksToInclude.clear();
            pipelineInfo.shaderInfos[3].conditionBlocksToInclude.clear();
			pipelineInfo.shaderInfos.resize(5);
			pipelineInfo.shaderInfos[4].shaderFilename = "Shaders/surfaceCoating/shader.frag";
			pipelineInfo.shaderInfos[4].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

			// Resources
			pipelineInfo.bindlessDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_BINDLESS;
			pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
			pipelineInfo.customMask = AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO | AdditionalDescriptorSetsMaskBits::GLOBAL_IRRADIANCE_SHADOW_MASK_INFO;

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(Wolf::DynamicState::VIEWPORT);
			pipelineInfo.enableDepthWrite = false;
			pipelineInfo.depthCompareOp = Wolf::CompareOp::EQUAL;

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);

			// Output Ids
			pipelineInfo.bindlessDescriptorSlot = -1;
			pipelineInfo.lightDescriptorSlot = -1;
            pipelineInfo.customMask = 0;
            pipelineInfo.shaderInfos[1].conditionBlocksToInclude.emplace_back("NO_LIGHTING");
            pipelineInfo.shaderInfos[2].conditionBlocksToInclude.emplace_back("NO_LIGHTING");
            pipelineInfo.shaderInfos[3].conditionBlocksToInclude.emplace_back("NO_LIGHTING");
			pipelineInfo.shaderInfos[4].shaderFilename = "Shaders/surfaceCoating/outputIds.frag";
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_OUTPUT_IDS);

			// Custom depth
			// pipelineInfo.bindlessDescriptorSlot = -1;
			// pipelineInfo.lightDescriptorSlot = -1;
			// pipelineInfo.shaderInfos.resize(1);
			// pipelineInfo.dynamicStates.clear();
			// pipelineInfo.enableDepthWrite = true;
			// pipelineInfo.depthCompareOp = Wolf::CompareOp::LESS_OR_EQUAL;
			// pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_CUSTOM_DEPTH);
            pipelineSet->addEmptyPipeline(CommonPipelineIndices::PIPELINE_IDX_CUSTOM_RENDER); // little hack to not add snow on top of the snow
    }));

    // Triangle list
    //m_mesh.reset(new Wolf::NoVertexMesh(5766)); // 32 * 32 points -> 31 * 31 quads = 961 -> 6 vertices per quad -> 961 * 6 = 5766

    // Triangle strip
    // constexpr uint32_t width = 32;
    // constexpr uint32_t height = 32;
    // constexpr uint32_t vertexCount = (height - 1) * (2 * width) + (height - 2) * 2;
    // m_mesh.reset(new Wolf::NoVertexMesh(vertexCount));

    // Tessellation (quads)
    m_mesh.reset(new Wolf::NoVertexMesh(31 * 31 * 4));

    m_renderingPipeline->getSurfaceCoatingDataPreparationPass()->registerComponent(this);
}

SurfaceCoatingComponent::~SurfaceCoatingComponent() = default;

void SurfaceCoatingComponent::loadParams(Wolf::JSONReader& jsonReader)
{
    ::loadParams<PatternImageArrayItem>(jsonReader, ID, m_editorParams);
}

void SurfaceCoatingComponent::activateParams()
{
    for (EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->activate();
    }
}

void SurfaceCoatingComponent::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
    for (const EditorParamInterface* editorParam : m_editorParams)
    {
        editorParam->addToJSON(outJSON, tabCount, false);
    }
}

void SurfaceCoatingComponent::updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
    bool isCustomRenderRequestRunning = m_registeredCustomRenderId != CustomSceneRenderPass::NO_REQUEST_ID && m_customRenderPass->isRequestRunning(m_registeredCustomRenderId);

    if (m_customRenderImagesCreationRequested)
    {
        if (isCustomRenderRequestRunning)
        {
            m_customRenderPass->pauseRequestBeforeFrame(m_registeredCustomRenderId);
            m_renderingPipeline->getSurfaceCoatingDataPreparationPass()->unregisterComponent(this);
        }
        else
        {
            createCustomRenderImages();
            m_customRenderImagesCreationRequested = false;
            m_renderingPipeline->getSurfaceCoatingDataPreparationPass()->registerComponent(this);

            if (m_registeredCustomRenderId != CustomSceneRenderPass::NO_REQUEST_ID)
            {
                m_customRenderPass->unpauseRequestBeforeFrame(m_registeredCustomRenderId);
            }
        }
    }

    if (m_depthCameraCreationRequested)
    {
        if (isCustomRenderRequestRunning)
        {
            m_customRenderPass->pauseRequestBeforeFrame(m_registeredCustomRenderId);
        }
        else
        {
            createDepthCamera();
            m_depthCameraCreationRequested = false;

            if (m_registeredCustomRenderId != CustomSceneRenderPass::NO_REQUEST_ID)
            {
                m_customRenderPass->unpauseRequestBeforeFrame(m_registeredCustomRenderId);
            }
        }
    }

    if (m_needToRegisterCustomRenderImages)
    {
        CustomSceneRenderPass::Request request(m_depthImage.createNonOwnerResource(), m_depthCamera.createNonOwnerResource<Wolf::CameraInterface>(),
            { { m_normalImage.createNonOwnerResource(), GameContext::DisplayType::VERTEX_NORMAL } });

        if (m_registeredCustomRenderId == -1)
        {
            m_registeredCustomRenderId = m_customRenderPass->addRequestBeforeFrame(request);;
        }
        else
        {
            m_customRenderPass->updateRequestBeforeFrame(m_registeredCustomRenderId, request);
        }

        m_needToRegisterCustomRenderImages = false;
    }

    m_patternsImageUpdateRequestedLock.lock();
    patternImagesSanityChecks();
    for (int32_t i = static_cast<int32_t>(m_patternsImageUpdateRequested.size()) - 1; i >= 0; i--)
    {
        uint32_t patternImageIdx = m_patternsImageUpdateRequested[i];
        if (!m_patternImages[patternImageIdx].hasHeightImage() || m_patternImages[patternImageIdx].isHeightImageLoaded())
        {
            updatePatternInBindless(patternImageIdx);
            m_patternsImageUpdateRequested.erase(m_patternsImageUpdateRequested.begin() + i);
        }
    }
    m_patternsImageUpdateRequestedLock.unlock();

    if (m_patchBoundsDebugDataRebuildRequested)
    {
        std::vector<uint8_t> patchBoundsDebugData;
        m_patchBoundsImage->exportToBuffer(patchBoundsDebugData);

        short* patchBoundsData = reinterpret_cast<short*>(patchBoundsDebugData.data());
        for (uint32_t i = 0; i < 32; i++)
        {
            for (uint32_t j = 0; j < 32; j++)
            {
                uint32_t dataIdx = i + j * 32;

                short min16bit = patchBoundsData[dataIdx * 2 + 1];
                float minFloat = (1.0f - glm::detail::toFloat32(min16bit)) * m_depthScale + m_depthOffset + m_geometryVerticalOffset;

                short max16bit = patchBoundsData[dataIdx * 2];
                float maxFloat = (1.0f - glm::detail::toFloat32(max16bit)) * m_depthScale + m_depthOffset + m_geometryVerticalOffset + m_globalThickness;

                glm::vec4 minPos = computeCornerPos(0, glm::uvec2(i, j));
                glm::vec4 maxPos = computeCornerPos(3, glm::uvec2(i, j));
                Wolf::AABB aabb(glm::vec3(minPos.x, minFloat, minPos.z), glm::vec3(maxPos.x, maxFloat, maxPos.z));

                m_patchBoundsAABBs[dataIdx] = aabb;
            }
        }

        m_patchBoundsDebugDataRebuildRequested = false;
    }

    UniformBufferData ubData{};
    ubData.viewProjectionMatrix = m_depthCamera->getProjectionMatrix() * m_depthCamera->getViewMatrix();

    Wolf::AABB aabb = computeRangeAABB();
    ubData.yMin = aabb.getMin().y;
    ubData.yMax = aabb.getMax().y;
    ubData.verticalOffset = m_geometryVerticalOffset;
    ubData.globalThickness = m_globalThickness;
    ubData.patternImageCount = m_patternImages.size();

    const Wolf::Viewport renderViewport = m_renderingPipeline->getRenderViewport();
    ubData.viewportWidth = static_cast<uint32_t>(renderViewport.width);
    ubData.viewportHeight = static_cast<uint32_t>(renderViewport.height);

    ubData.depthScale = m_depthScale;
    ubData.depthOffset = m_depthOffset;

    for (uint32_t i = 0; i < ubData.patternImageCount; i++)
    {
        ubData.materialIndices[i] = m_patternImages[i].getMaterialIdx();
        ubData.patternScales[i] = m_patternImages[i].getPatternScale();;
    }

    m_uniformBuffer->transferCPUMemory(&ubData, sizeof(UniformBufferData));
}

void SurfaceCoatingComponent::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
    if (m_enableGlobalAABBDebug)
    {
        debugRenderingManager.addAABB(computeRangeAABB());
    }

    if (m_enablePatchAABBDebug)
    {
        for (const Wolf::AABB& aabb : m_patchBoundsAABBs)
        {
            debugRenderingManager.addAABB(aabb);
        }
    }
}

bool SurfaceCoatingComponent::getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList)
{
    Wolf::RenderMeshList::MeshToRender meshToRenderInfo = { m_mesh.createNonOwnerResource<Wolf::MeshInterface>(), m_defaultPipelineSet->getResource().createConstNonOwnerResource() };

    Wolf::DescriptorSetBindInfo descriptorSetBindInfoNoLighting(m_descriptorSet.createConstNonOwnerResource(), m_descriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_PASS_INFO);
    meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH].push_back(descriptorSetBindInfoNoLighting);
    meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP].push_back(descriptorSetBindInfoNoLighting);
    meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_OUTPUT_IDS].push_back(descriptorSetBindInfoNoLighting);

    Wolf::DescriptorSetBindInfo descriptorSetBindInfoLighting(m_descriptorSet.createConstNonOwnerResource(), m_descriptorSetLayout.createConstNonOwnerResource(), DescriptorSetSlots::DESCRIPTOR_SET_SLOT_COUNT);
    meshToRenderInfo.perPipelineDescriptorSets[CommonPipelineIndices::PIPELINE_IDX_FORWARD].push_back(descriptorSetBindInfoLighting);

    InstanceData instanceData{};

    Wolf::AABB aabb = computeRangeAABB();
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, aabb.getCenter());
    transform = glm::scale(transform, aabb.getSize());
    instanceData.transform = transform;

    instanceData.firstMaterialIdx = 0; // TODO
    instanceData.entityIdx = m_entity->getIdx();
    outList.push_back({ meshToRenderInfo, instanceData });

    return true;
}

bool SurfaceCoatingComponent::getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos)
{
    return EditorModelInterface::getInstancesForRayTracedWorld(instanceInfos);
}

bool SurfaceCoatingComponent::getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList)
{
    return true;
}

Wolf::AABB SurfaceCoatingComponent::getAABB() const
{
    return computeRangeAABB();
}

Wolf::BoundingSphere SurfaceCoatingComponent::getBoundingSphere() const
{
    Wolf::AABB aabb = computeRangeAABB();
    return Wolf::BoundingSphere(aabb.getCenter(), aabb.getSize().x * 0.5f);
}

Wolf::ResourceNonOwner<Wolf::Image> SurfaceCoatingComponent::getDepthImage()
{
    return m_depthImage.createNonOwnerResource();
}

Wolf::ResourceNonOwner<Wolf::Image> SurfaceCoatingComponent::getPatchBoundsImage()
{
    return m_patchBoundsImage.createNonOwnerResource();
}

Wolf::ResourceNonOwner<Wolf::Sampler> SurfaceCoatingComponent::getSampler()
{
    return m_sampler.createNonOwnerResource();
}

glm::mat4 SurfaceCoatingComponent::getDepthViewProj() const
{
    return m_depthCamera->getProjectionMatrix() * m_depthCamera->getViewMatrix();
}

glm::mat4 SurfaceCoatingComponent::computeTransform()
{
    Wolf::AABB aabb = computeRangeAABB();

    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, aabb.getCenter());
    transform = glm::scale(transform, aabb.getSize());

    if (getTransform() != glm::mat4(1.0f))
    {
        Wolf::Debug::sendCriticalError("Unhandled transform");
    }

    return transform;
}

glm::vec4 SurfaceCoatingComponent::computeCornerPos(uint32_t cornerIdx, glm::uvec2 gridCoords)
{
    glm::vec2 offsets[4] =
    {
        glm::vec2(0.0, 0.0), glm::vec2(1.0, 0.0),
        glm::vec2(0.0, 1.0), glm::vec2(1.0, 1.0)
    };

    glm::vec2 uv = (glm::vec2(static_cast<float>(gridCoords.x), static_cast<float>(gridCoords.y)) + offsets[cornerIdx]) / 32.0f;
    glm::vec3 localPos = glm::vec3(uv.x - 0.5f, 0.0, uv.y - 0.5f);
    return computeTransform() * glm::vec4(localPos, 1.0);
}

void SurfaceCoatingComponent::onCustomRenderResolutionChanged()
{
    if (m_depthImage && m_customRenderResolution != m_depthImage->getExtent().width)
    {
        m_customRenderImagesCreationRequested = true;
    }
}

SurfaceCoatingComponent::PatternImageArrayItem::PatternImageArrayItem() : ParameterGroupInterface(TAB)
{
    m_name = DEFAULT_NAME;
    m_patternScale = glm::vec2(1.0f);
}

void SurfaceCoatingComponent::PatternImageArrayItem::getAllParams(std::vector<EditorParamInterface*>& out) const
{
    std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

void SurfaceCoatingComponent::PatternImageArrayItem::getAllVisibleParams(std::vector<EditorParamInterface*>& out) const
{
    std::copy(m_editorParams.data(), &m_editorParams.back() + 1, std::back_inserter(out));
}

bool SurfaceCoatingComponent::PatternImageArrayItem::hasDefaultName() const
{
    return std::string(m_name) == DEFAULT_NAME;
}

void SurfaceCoatingComponent::PatternImageArrayItem::setResourceManager(const Wolf::ResourceNonOwner<AssetManager>& resourceManager)
{
    m_resourceManager = resourceManager;
}

void SurfaceCoatingComponent::PatternImageArrayItem::setGetEntityFromLoadingPathCallback(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
{
    m_getEntityFromLoadingPathCallback = getEntityFromLoadingPathCallback;
}

bool SurfaceCoatingComponent::PatternImageArrayItem::hasHeightImage() const
{
    return m_patternImageHeightAssetId != NO_ASSET;
}

bool SurfaceCoatingComponent::PatternImageArrayItem::isHeightImageLoaded() const
{
    return m_patternImageHeightAssetId == NO_ASSET || m_resourceManager->isImageLoaded(m_patternImageHeightAssetId);
}

Wolf::ResourceNonOwner<Wolf::Image> SurfaceCoatingComponent::PatternImageArrayItem::getHeightImage()
{
    return m_resourceManager->getImage(m_patternImageHeightAssetId);
}

void SurfaceCoatingComponent::PatternImageArrayItem::removeHeightImage()
{
    m_patternImageHeightAssetId = NO_ASSET;
}

void SurfaceCoatingComponent::PatternImageArrayItem::onPatternImageHeightChanged()
{
    if (static_cast<std::string>(m_patternImageHeight) != "")
    {
        m_patternImageHeightAssetId = m_resourceManager->addImage(m_patternImageHeight, false, Wolf::Format::R8G8B8A8_UNORM, false, false);
    }
    notifySubscribers();
}

void SurfaceCoatingComponent::PatternImageArrayItem::onMaterialEntityChanged()
{
    if (static_cast<std::string>(m_materialEntityParam).empty())
    {
        m_materialEntity = Wolf::NullableResourceNonOwner<Entity>();
        return;
    }

    m_materialEntity = m_getEntityFromLoadingPathCallback(m_materialEntityParam);
    if (Wolf::NullableResourceNonOwner<MaterialComponent> materialComponent = m_materialEntity->getComponent<MaterialComponent>())
    {
        if (!materialComponent->isSubscribed(this))
        {
            materialComponent->subscribe(this, [this](Flags) { updateMaterialInfo();});
        }
    }

    // Subscribe to be notified if a material component is added to the entity
    if (!m_materialEntity->isSubscribed(this))
    {
        m_materialEntity->subscribe(this, [this](Flags)
        {
            onMaterialEntityChanged();
        });
    }

    updateMaterialInfo();
}

void SurfaceCoatingComponent::PatternImageArrayItem::updateMaterialInfo()
{
    if (Wolf::NullableResourceNonOwner<MaterialComponent> materialComponent = m_materialEntity->getComponent<MaterialComponent>())
    {
        // Sanity checks
        uint32_t textureSetCountInMaterial = materialComponent->getTextureSetCount();
        for (uint32_t i = 0; i < textureSetCountInMaterial; i++)
        {
            if (Wolf::NullableResourceNonOwner<TextureSetComponent> textureSetComponent = materialComponent->getTextureSetComponent(i))
            {
                if (textureSetComponent->getSamplingMode() == static_cast<uint32_t>(Wolf::MaterialsGPUManager::TextureSetInfo::SamplingMode::TRIPLANAR))
                {
                    glm::vec3 triplanarScale = textureSetComponent->getTriplanarScale();
                }
                else
                {
                    Wolf::Debug::sendError("Surface coating pattern image: all texture set sampling modes should be triplanar");
                }
            }
        }

        m_materialIdx = materialComponent->getMaterialIdx();

    }
    else
    {
        m_materialIdx = 0;
    }
}

void SurfaceCoatingComponent::onPatternImageAdded()
{
    m_patternImages.back().setResourceManager(m_resourceManager);
    m_patternImages.back().setGetEntityFromLoadingPathCallback(m_getEntityFromLoadingPathCallback);
    uint32_t patterImageIdx = m_patternImages.size() - 1;
    m_patternImages.back().subscribe(this, [this, patterImageIdx](Flags) { onPatternImageChanged(patterImageIdx); });

    m_requestReloadCallback(this);
}

void SurfaceCoatingComponent::onPatternImageChanged(uint32_t idx)
{
    m_patternsImageUpdateRequestedLock.lock();
    m_patternsImageUpdateRequested.push_back(idx);
    m_patternsImageUpdateRequestedLock.unlock();
}

Wolf::AABB SurfaceCoatingComponent::computeRangeAABB() const
{
    return Wolf::AABB(m_rangeAABBMin, m_rangeAABBMax);
}

void SurfaceCoatingComponent::onRangeChanged()
{
    m_depthCameraCreationRequested = true;
}

void SurfaceCoatingComponent::onEnablePatchAABBDebug()
{
    m_patchBoundsDebugDataRebuildRequested = true;
}

void SurfaceCoatingComponent::createCustomRenderImages()
{
    Wolf::CreateImageInfo createDepthImageInfo{};
    createDepthImageInfo.extent = { m_customRenderResolution, m_customRenderResolution, 1 };
    createDepthImageInfo.format = Wolf::Format::D32_SFLOAT;
    createDepthImageInfo.usage = Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | Wolf::ImageUsageFlagBits::SAMPLED;
    createDepthImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::DEPTH;
    createDepthImageInfo.mipLevelCount = 1;
    m_depthImage.reset(Wolf::Image::createImage(createDepthImageInfo));
    m_depthImage->setImageLayout(Wolf::Image::TransitionLayoutInfo(VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 1, 0,
        1, VK_IMAGE_LAYOUT_UNDEFINED));

    Wolf::CreateImageInfo createNormalImageInfo{};
    createNormalImageInfo.extent = { m_customRenderResolution, m_customRenderResolution, 1 };
    createNormalImageInfo.format = Wolf::Format::R16G16B16A16_SFLOAT;
    createNormalImageInfo.usage = Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT | Wolf::ImageUsageFlagBits::SAMPLED;
    createNormalImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::COLOR;
    createNormalImageInfo.mipLevelCount = 1;
    m_normalImage.reset(Wolf::Image::createImage(createNormalImageInfo));
    m_normalImage->setImageLayout(Wolf::Image::TransitionLayoutInfo(VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, 0,
        1, VK_IMAGE_LAYOUT_UNDEFINED));

    m_needToRegisterCustomRenderImages = true;
    createOrUpdateDescriptorSet();
}

void SurfaceCoatingComponent::createDepthCamera()
{
    // TODO: recreating the camera is overkill and needs a new descriptor set, we should update the existing one
    Wolf::AABB rangeAABB = computeRangeAABB();
    m_depthCamera.reset(new Wolf::OrthographicCamera(rangeAABB.getCenter(), rangeAABB.getSize().x * 0.5f, rangeAABB.getSize().y * 0.5, glm::vec3(0.0f, -1.0f, 0.0f),
        0.0f, rangeAABB.getSize().y));

    m_depthScale = rangeAABB.getSize().y;
    m_depthOffset = rangeAABB.getCenter().y - m_depthScale * 0.5f;

    m_needToRegisterCustomRenderImages = true;
}

void SurfaceCoatingComponent::createOrUpdateDescriptorSet()
{
    Wolf::DescriptorSetGenerator descriptorSetGenerator(m_descriptorSetLayoutGenerator.getDescriptorLayouts());

    Wolf::DescriptorSetGenerator::ImageDescription depthImageDesc{};
    depthImageDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    depthImageDesc.imageView = m_depthImage->getDefaultImageView();
    descriptorSetGenerator.setImage(0, depthImageDesc);

    Wolf::DescriptorSetGenerator::ImageDescription normalImageDesc{};
    normalImageDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    normalImageDesc.imageView = m_normalImage->getDefaultImageView();
    descriptorSetGenerator.setImage(1, normalImageDesc);

    descriptorSetGenerator.setSampler(2, *m_sampler);
    descriptorSetGenerator.setUniformBuffer(3, *m_uniformBuffer);

    std::vector<Wolf::DescriptorSetGenerator::ImageDescription> defaultParticleDepthImages(1);
    defaultParticleDepthImages[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    defaultParticleDepthImages[0].imageView = m_defaultPatternHeightImage->getDefaultImageView();
    descriptorSetGenerator.setImages(4, defaultParticleDepthImages);

    Wolf::DescriptorSetGenerator::ImageDescription patternIdxImageDesc{};
    patternIdxImageDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    patternIdxImageDesc.imageView = m_patternIdxImage->getDefaultImageView();
    descriptorSetGenerator.setImage(5, patternIdxImageDesc);

    Wolf::DescriptorSetGenerator::ImageDescription patchBoundsImageDesc{};
    patchBoundsImageDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    patchBoundsImageDesc.imageView = m_patchBoundsImage->getDefaultImageView();
    descriptorSetGenerator.setImage(6, patchBoundsImageDesc);

    if (!m_descriptorSet)
    {
        m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
    }
    m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void SurfaceCoatingComponent::updatePatternInBindless(uint32_t patternIdx)
{
    Wolf::DescriptorSetUpdateInfo descriptorSetUpdateInfo;
    descriptorSetUpdateInfo.descriptorImages.resize(1);

    {
        descriptorSetUpdateInfo.descriptorImages[0].images.resize(1);
        Wolf::DescriptorSetUpdateInfo::ImageData& imageData = descriptorSetUpdateInfo.descriptorImages[0].images.back();
        imageData.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageData.imageView = getPatternHeightImage(patternIdx)->getDefaultImageView();

        Wolf::DescriptorLayout& descriptorLayout = descriptorSetUpdateInfo.descriptorImages[0].descriptorLayout;
        descriptorLayout.binding = 4;
        descriptorLayout.arrayIndex = patternIdx;
        descriptorLayout.descriptorType = Wolf::DescriptorType::SAMPLED_IMAGE;
    }

    m_descriptorSet->update(descriptorSetUpdateInfo);
}

void SurfaceCoatingComponent::createSurfaceCoatingSpecificImages()
{
    Wolf::CreateImageInfo createPatchBoundsImageInfo{};
    createPatchBoundsImageInfo.extent = { 32, 32, 1 }; // grid size
    createPatchBoundsImageInfo.format = Wolf::Format::R16G16_SFLOAT;
    createPatchBoundsImageInfo.usage = Wolf::ImageUsageFlagBits::STORAGE | Wolf::ImageUsageFlagBits::SAMPLED;
    createPatchBoundsImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::COLOR;
    createPatchBoundsImageInfo.mipLevelCount = 1;
    m_patchBoundsImage.reset(Wolf::Image::createImage(createPatchBoundsImageInfo));
    m_patchBoundsImage->setImageLayout(Wolf::Image::TransitionLayoutInfo(VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, 0,
        1, VK_IMAGE_LAYOUT_UNDEFINED));

}

void SurfaceCoatingComponent::patternImagesSanityChecks()
{
    uint32_t patternImageSize = 0;
    for (uint32_t patternImageIdx = 0; patternImageIdx < m_patternImages.size(); patternImageIdx++)
    {
        if (!m_patternImages[patternImageIdx].isHeightImageLoaded())
        {
            continue;
        }

        if (m_patternImages[patternImageIdx].hasHeightImage())
        {
            Wolf::ResourceNonOwner<Wolf::Image> image = m_patternImages[patternImageIdx].getHeightImage();
            if (image->getExtent().width != image->getExtent().height)
            {
                Wolf::Debug::sendError("Pattern images must be squares, switching to default");
                m_patternImages[patternImageIdx].removeHeightImage();
            }
            else
            {
                if (patternImageSize != 0 && patternImageSize != image->getExtent().width)
                {
                    Wolf::Debug::sendError("Pattern images must have the same size, switching to default");
                    m_patternImages[patternImageIdx].removeHeightImage();
                }
                else
                {
                    patternImageSize = image->getExtent().width;
                }
            }
        }
    }
}

Wolf::ResourceNonOwner<Wolf::Image> SurfaceCoatingComponent::getPatternHeightImage(uint32_t imageIdx)
{
    if (m_patternImages.size() <= imageIdx || !m_patternImages[imageIdx].hasHeightImage() || !m_patternImages[imageIdx].isHeightImageLoaded())
    {
        return m_defaultPatternHeightImage.createNonOwnerResource();
    }

    return m_patternImages[imageIdx].getHeightImage();
}

void SurfaceCoatingComponent::createDefaultPatternImage()
{
    Wolf::CreateImageInfo createImageInfo{};
    createImageInfo.extent = { 1, 1, 1 };
    createImageInfo.format = Wolf::Format::R32_SFLOAT;
    createImageInfo.usage = Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::TRANSFER_DST;
    createImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::COLOR;
    createImageInfo.mipLevelCount = 1;
    m_defaultPatternHeightImage.reset(Wolf::Image::createImage(createImageInfo));

    float defaultData = 1.0f;
    m_defaultPatternHeightImage->copyCPUBuffer(reinterpret_cast<const unsigned char*>(&defaultData), Wolf::Image::SampledInFragmentShader());
}

void SurfaceCoatingComponent::createPatternIdxImage()
{
    Wolf::CreateImageInfo createImageInfo{};
    createImageInfo.extent = { 32, 32, 1 }; // this is the size of the grid
    createImageInfo.format = Wolf::Format::R32_UINT;
    createImageInfo.usage = Wolf::ImageUsageFlagBits::STORAGE | Wolf::ImageUsageFlagBits::TRANSFER_DST;
    createImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::COLOR;
    createImageInfo.mipLevelCount = 1;
    m_patternIdxImage.reset(Wolf::Image::createImage(createImageInfo));

    static std::default_random_engine generator;
    generator.seed(0);
    static std::uniform_real_distribution distrib(0.0f, static_cast<float>(MAX_PATTERNS + 0.99f));

    std::vector<uint32_t> randomData(createImageInfo.extent.width * createImageInfo.extent.height);
    for (uint32_t& random : randomData)
    {
        random = static_cast<uint32_t>(distrib(generator));
    }
    m_patternIdxImage->copyCPUBuffer(reinterpret_cast<const unsigned char*>(randomData.data()), { VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
        0, 1, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED });
}

