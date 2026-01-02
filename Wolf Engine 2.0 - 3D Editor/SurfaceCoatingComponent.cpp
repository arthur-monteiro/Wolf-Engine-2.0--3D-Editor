#include "SurfaceCoatingComponent.h"
#include "SurfaceCoatingComponent.h"

#include <Pipeline.h>
#include <random>

#include "CommonLayouts.h"
#include "DebugRenderingManager.h"
#include "EditorParamsHelper.h"
#include "MaterialComponent.h"

SurfaceCoatingComponent::SurfaceCoatingComponent(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<AssetManager>& resourceManager,
                                                 std::function<void(ComponentInterface*)> requestReloadCallback, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback)
: m_customRenderPass(renderingPipeline->getCustomRenderPass()), m_resourceManager(resourceManager), m_requestReloadCallback(requestReloadCallback), m_getEntityFromLoadingPathCallback(getEntityFromLoadingPathCallback)
{
    Wolf::ShaderStageFlags resourcesAccessibility = Wolf::ShaderStageFlagBits::TESSELLATION_EVALUATION | Wolf::ShaderStageFlagBits::GEOMETRY;
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, resourcesAccessibility, 0, 1); // Depth texture
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, resourcesAccessibility, 1, 1); // Normal texture
    m_descriptorSetLayoutGenerator.addSampler(resourcesAccessibility, 2); // Sampler
    m_descriptorSetLayoutGenerator.addUniformBuffer(resourcesAccessibility, 3); // Uniform buffer
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::SAMPLED_IMAGE, resourcesAccessibility, 4, MAX_PATTERNS * 2,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT); // Pattern images
    m_descriptorSetLayoutGenerator.addImages(Wolf::DescriptorType::STORAGE_IMAGE, resourcesAccessibility, 5, 1); // Pattern index image
    m_descriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(m_descriptorSetLayoutGenerator.getDescriptorLayouts(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT));

    m_sampler.reset(Wolf::Sampler::createSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR));
    m_uniformBuffer.reset(new Wolf::UniformBuffer(sizeof(UniformBufferData)));

    createDefaultPatternImage();
    createPatternIdxImage();

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

            pipelineInfo.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

            pipelineInfo.patchControlPoint = 4;

			// Resources
			pipelineInfo.cameraDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_CAMERA;

			// Color Blend
			pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_PRE_DEPTH);

			/* Shadow maps */
			pipelineInfo.dynamicStates.clear();
			pipelineInfo.depthBiasConstantFactor = 4.0f;
			pipelineInfo.depthBiasSlopeFactor = 2.5f;
			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_SHADOW_MAP);
			pipelineInfo.depthBiasConstantFactor = 0.0f;
			pipelineInfo.depthBiasSlopeFactor = 0.0f;

			/* Forward */
            pipelineInfo.shaderInfos[2].conditionBlocksToInclude.clear();
            pipelineInfo.shaderInfos[3].conditionBlocksToInclude.clear();
			pipelineInfo.shaderInfos.resize(5);
			pipelineInfo.shaderInfos[4].shaderFilename = "Shaders/defaultPipeline/shader.frag";
			pipelineInfo.shaderInfos[4].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

			// Resources
			pipelineInfo.bindlessDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_BINDLESS;
			pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
			pipelineInfo.customMask = AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO | AdditionalDescriptorSetsMaskBits::GLOBAL_IRRADIANCE_SHADOW_MASK_INFO;

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
			pipelineInfo.enableDepthWrite = false;
			pipelineInfo.depthCompareOp = Wolf::CompareOp::EQUAL;

			pipelineSet->addPipeline(pipelineInfo, CommonPipelineIndices::PIPELINE_IDX_FORWARD);

			// Output Ids
			pipelineInfo.bindlessDescriptorSlot = -1;
			pipelineInfo.lightDescriptorSlot = -1;
            pipelineInfo.customMask = 0;
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
        }
        else
        {
            createCustomRenderImages();
            m_customRenderImagesCreationRequested = false;

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
        if ((!m_patternImages[patternImageIdx].hasHeightImage() || m_patternImages[patternImageIdx].isHeightImageLoaded()) &&
            (!m_patternImages[patternImageIdx].hasNormalImage() || m_patternImages[patternImageIdx].isNormalImageLoaded()))
        {
            updatePatternInBindless(patternImageIdx);
            m_patternsImageUpdateRequested.erase(m_patternsImageUpdateRequested.begin() + i);
        }
    }
    m_patternsImageUpdateRequestedLock.unlock();

    UniformBufferData ubData{};
    ubData.viewProjectionMatrix = m_depthCamera->getProjectionMatrix() * m_depthCamera->getViewMatrix();

    Wolf::AABB aabb = computeRangeAABB();
    ubData.yMin = aabb.getMin().y;
    ubData.yMax = aabb.getMax().y;
    ubData.verticalOffset = m_geometryVerticalOffset;
    ubData.globalThickness = m_globalThickness;
    ubData.patternImageCount = m_patternImages.size();
    ubData.materialIdx = m_materialIdx;

    m_uniformBuffer->transferCPUMemory(&ubData, sizeof(UniformBufferData));
}

void SurfaceCoatingComponent::addDebugInfo(DebugRenderingManager& debugRenderingManager)
{
    if (m_enableAABBDebug)
    {
        debugRenderingManager.addAABB(computeRangeAABB());
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

void SurfaceCoatingComponent::onCustomRenderResolutionChanged()
{
    if (m_depthImage && m_customRenderResolution != m_depthImage->getExtent().width)
    {
        m_customRenderImagesCreationRequested = true;
    }
}

SurfaceCoatingComponent::PatternImageArrayItem::PatternImageArrayItem() : ParameterGroupInterface(TAB)
{
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

bool SurfaceCoatingComponent::PatternImageArrayItem::hasHeightImage() const
{
    return m_patternImageHeightAssetId != NO_ASSET;
}

bool SurfaceCoatingComponent::PatternImageArrayItem::hasNormalImage() const
{
    return m_patternImageNormalAssetId != NO_ASSET;
}

bool SurfaceCoatingComponent::PatternImageArrayItem::isHeightImageLoaded() const
{
    return m_patternImageHeightAssetId == NO_ASSET || m_resourceManager->isImageLoaded(m_patternImageHeightAssetId);
}

bool SurfaceCoatingComponent::PatternImageArrayItem::isNormalImageLoaded() const
{
    return m_patternImageNormalAssetId == NO_ASSET || m_resourceManager->isImageLoaded(m_patternImageNormalAssetId);
}

Wolf::ResourceNonOwner<Wolf::Image> SurfaceCoatingComponent::PatternImageArrayItem::getHeightImage()
{
    return m_resourceManager->getImage(m_patternImageHeightAssetId);
}

Wolf::ResourceNonOwner<Wolf::Image> SurfaceCoatingComponent::PatternImageArrayItem::getNormalImage()
{
    return m_resourceManager->getImage(m_patternImageNormalAssetId);
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

void SurfaceCoatingComponent::PatternImageArrayItem::onPatternImageNormalChanged()
{
    if (static_cast<std::string>(m_patternImageNormal) != "")
    {
        m_patternImageNormalAssetId = m_resourceManager->addImage(m_patternImageNormal, false, Wolf::Format::BC5_UNORM_BLOCK, false, false);
    }
    notifySubscribers();
}

void SurfaceCoatingComponent::onPatternImageAdded()
{
    m_patternImages.back().setResourceManager(m_resourceManager);
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

void SurfaceCoatingComponent::onMaterialEntityChanged()
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
            materialComponent->subscribe(this, [this](Flags) { updateMaterialIdx();});
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

    updateMaterialIdx();
}

void SurfaceCoatingComponent::createCustomRenderImages()
{
    Wolf::CreateImageInfo createDepthImageInfo{};
    createDepthImageInfo.extent = { m_customRenderResolution, m_customRenderResolution, 1 };
    createDepthImageInfo.format = Wolf::Format::D32_SFLOAT;
    createDepthImageInfo.usage = Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | Wolf::ImageUsageFlagBits::SAMPLED;
    createDepthImageInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    createDepthImageInfo.mipLevelCount = 1;
    m_depthImage.reset(Wolf::Image::createImage(createDepthImageInfo));

    Wolf::CreateImageInfo createNormalImageInfo{};
    createNormalImageInfo.extent = { m_customRenderResolution, m_customRenderResolution, 1 };
    createNormalImageInfo.format = Wolf::Format::R16G16B16A16_SFLOAT;
    createNormalImageInfo.usage = Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT | Wolf::ImageUsageFlagBits::SAMPLED;
    createNormalImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    createNormalImageInfo.mipLevelCount = 1;
    m_normalImage.reset(Wolf::Image::createImage(createNormalImageInfo));

    m_needToRegisterCustomRenderImages = true;
    createOrUpdateDescriptorSet();
}

void SurfaceCoatingComponent::createDepthCamera()
{
    // TODO: recreating the camera is overkill and needs a new descriptor set, we should update the existing one
    Wolf::AABB rangeAABB = computeRangeAABB();
    m_depthCamera.reset(new Wolf::OrthographicCamera(rangeAABB.getCenter(), rangeAABB.getSize().x * 0.5f, rangeAABB.getSize().y * 0.5, glm::vec3(0.0f, -1.0f, 0.0f),
        0.0f, rangeAABB.getSize().y));

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

    if (!m_descriptorSet)
    {
        m_descriptorSet.reset(Wolf::DescriptorSet::createDescriptorSet(*m_descriptorSetLayout));
    }
    m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void SurfaceCoatingComponent::updatePatternInBindless(uint32_t patternIdx)
{
    Wolf::DescriptorSetUpdateInfo descriptorSetUpdateInfo;
    descriptorSetUpdateInfo.descriptorImages.resize(2);

    {
        descriptorSetUpdateInfo.descriptorImages[0].images.resize(1);
        Wolf::DescriptorSetUpdateInfo::ImageData& imageData = descriptorSetUpdateInfo.descriptorImages[0].images.back();
        imageData.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageData.imageView = getPatternHeightImage(patternIdx)->getDefaultImageView();

        Wolf::DescriptorLayout& descriptorLayout = descriptorSetUpdateInfo.descriptorImages[0].descriptorLayout;
        descriptorLayout.binding = 4;
        descriptorLayout.arrayIndex = patternIdx * 2;
        descriptorLayout.descriptorType = Wolf::DescriptorType::SAMPLED_IMAGE;
    }

    {
        descriptorSetUpdateInfo.descriptorImages[1].images.resize(1);
        Wolf::DescriptorSetUpdateInfo::ImageData& imageData = descriptorSetUpdateInfo.descriptorImages[1].images.back();
        imageData.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageData.imageView = getPatternNormalImage(patternIdx)->getDefaultImageView();

        Wolf::DescriptorLayout& descriptorLayout = descriptorSetUpdateInfo.descriptorImages[1].descriptorLayout;
        descriptorLayout.binding = 4;
        descriptorLayout.arrayIndex = patternIdx * 2 + 1;
        descriptorLayout.descriptorType = Wolf::DescriptorType::SAMPLED_IMAGE;
    }

    m_descriptorSet->update(descriptorSetUpdateInfo);
}

void SurfaceCoatingComponent::updateMaterialIdx()
{
    if (Wolf::NullableResourceNonOwner<MaterialComponent> materialComponent = m_materialEntity->getComponent<MaterialComponent>())
    {
        // TODO: check that all texture sets registered in material have "triplanar" sampling mode
        m_materialIdx = materialComponent->getMaterialIdx();
    }
    else
    {
        m_materialIdx = 0;
    }
}

void SurfaceCoatingComponent::patternImagesSanityChecks()
{
    // TODO: check also normal image

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

Wolf::ResourceNonOwner<Wolf::Image> SurfaceCoatingComponent::getPatternNormalImage(uint32_t imageIdx)
{
    if (m_patternImages.size() <= imageIdx || !m_patternImages[imageIdx].hasNormalImage() || !m_patternImages[imageIdx].isNormalImageLoaded())
    {
        return m_defaultPatternHeightImage.createNonOwnerResource(); // TODO: use a default normal
    }

    return m_patternImages[imageIdx].getNormalImage();
}

void SurfaceCoatingComponent::createDefaultPatternImage()
{
    Wolf::CreateImageInfo createImageInfo{};
    createImageInfo.extent = { 1, 1, 1 };
    createImageInfo.format = Wolf::Format::R32_SFLOAT;
    createImageInfo.usage = Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::TRANSFER_DST;
    createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
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
    createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
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

