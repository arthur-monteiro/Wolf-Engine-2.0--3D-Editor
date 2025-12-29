#pragma once

#include <NoVertexMesh.h>
#include <OrthographicCamera.h>
#include <PipelineSet.h>

#include "ComponentInterface.h"
#include "CustomSceneRenderPass.h"
#include "EditorModelInterface.h"
#include "EditorTypes.h"
#include "EditorTypes.h"

class SurfaceCoatingComponent : public EditorModelInterface
{
public:
    static inline std::string ID = "surfaceCoating";
    std::string getId() const override { return ID; }

    SurfaceCoatingComponent(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const Wolf::ResourceNonOwner<AssetManager>& resourceManager,
        std::function<void(ComponentInterface*)> requestReloadCallback, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);
    ~SurfaceCoatingComponent() override;

    void loadParams(Wolf::JSONReader& jsonReader) override;
    void activateParams() override;
    void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

    void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
    void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
    void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

    void saveCustom() const override {}

    bool getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) override;
    bool getInstancesForRayTracedWorld(std::vector<RayTracedWorldManager::RayTracedWorldInfo::InstanceInfo>& instanceInfos) override;
    bool getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList) override;

    Wolf::AABB getAABB() const override;
    Wolf::BoundingSphere getBoundingSphere() const override;
    std::string getTypeString() override { return "surfaceCoating"; }

private:
    Wolf::ResourceNonOwner<CustomSceneRenderPass> m_customRenderPass;
    Wolf::ResourceNonOwner<AssetManager> m_resourceManager;
    std::function<void(ComponentInterface*)> m_requestReloadCallback;
    std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

    inline static const std::string TAB = "Surface coating";

    void onCustomRenderResolutionChanged();
    EditorParamUInt m_customRenderResolution = EditorParamUInt("Custom render images resolution", TAB, "Geometry", 16, 4096, [this]() { onCustomRenderResolutionChanged(); });

    class PatternImageArrayItem : public ParameterGroupInterface, public Notifier
    {
    public:
        PatternImageArrayItem();

        void getAllParams(std::vector<EditorParamInterface*>& out) const override;
        void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
        bool hasDefaultName() const override;

        void setResourceManager(const Wolf::ResourceNonOwner<AssetManager>& resourceManager);

        [[nodiscard]] bool hasHeightImage() const;
        [[nodiscard]] bool hasNormalImage() const;
        [[nodiscard]] bool isHeightImageLoaded() const;
        [[nodiscard]] bool isNormalImageLoaded() const;
        [[nodiscard]] Wolf::ResourceNonOwner<Wolf::Image> getHeightImage();
        [[nodiscard]] Wolf::ResourceNonOwner<Wolf::Image> getNormalImage();
        void removeHeightImage();

    private:
        inline static const std::string DEFAULT_NAME = "New pattern image";
        Wolf::NullableResourceNonOwner<AssetManager> m_resourceManager;

        void onPatternImageHeightChanged();
        AssetManager::AssetId m_patternImageHeightAssetId = AssetManager::NO_ASSET;
        EditorParamString m_patternImageHeight = EditorParamString("Pattern image height", TAB, "Pattern image", [this]() { onPatternImageHeightChanged(); }, EditorParamString::ParamStringType::FILE_IMG);
        void onPatternImageNormalChanged();
        AssetManager::AssetId m_patternImageNormalAssetId = AssetManager::NO_ASSET;
        EditorParamString m_patternImageNormal = EditorParamString("Pattern image normal", TAB, "Pattern image", [this]() { onPatternImageNormalChanged(); }, EditorParamString::ParamStringType::FILE_IMG);

        std::array<EditorParamInterface*, 2> m_editorParams =
        {
            &m_patternImageHeight,
            &m_patternImageNormal
        };
    };
    static constexpr uint32_t MAX_PATTERNS = 4;
    void onPatternImageAdded();
    void onPatternImageChanged(uint32_t idx);
    EditorParamArray<PatternImageArrayItem> m_patternImages = EditorParamArray<PatternImageArrayItem>("Pattern images", TAB, "Geometry", MAX_PATTERNS, [this] { onPatternImageAdded(); });
    EditorParamFloat m_geometryVerticalOffset = EditorParamFloat("Vertical offset", TAB, "Geometry", -1.0f, 1.0f);
    EditorParamFloat m_globalThickness = EditorParamFloat("Global thickness", TAB, "Geometry", 0.0f, 1.0f);

    Wolf::AABB computeRangeAABB() const;
    void onRangeChanged();
    EditorParamVector3 m_rangeAABBMin = EditorParamVector3("AABB min", TAB, "Range", -50.0f, 50.0f, [this]() { onRangeChanged(); });
    EditorParamVector3 m_rangeAABBMax = EditorParamVector3("AABB max", TAB, "Range", -50.0f, 50.0f, [this]() { onRangeChanged(); });

    void onMaterialEntityChanged();
    EditorParamString m_materialEntityParam = EditorParamString("Material entity", TAB, "Visual", [this]() { onMaterialEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);

    EditorParamBool m_enableAABBDebug = EditorParamBool("Enable AABB debug", TAB, "Debug");

    std::array<EditorParamInterface*, 8> m_editorParams =
    {
        &m_customRenderResolution,
        &m_patternImages,
        &m_geometryVerticalOffset,
        &m_globalThickness,

        &m_rangeAABBMin,
        &m_rangeAABBMax,

        &m_materialEntityParam,

        &m_enableAABBDebug
    };

    // Graphic resources
    std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, SurfaceCoatingComponent>> m_defaultPipelineSet;
    Wolf::ResourceUniqueOwner<Wolf::NoVertexMesh> m_mesh;

    bool m_needToRegisterCustomRenderImages = false;
    CustomSceneRenderPass::RequestId m_registeredCustomRenderId = CustomSceneRenderPass::NO_REQUEST_ID;

    void createCustomRenderImages();
    bool m_customRenderImagesCreationRequested = true;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_depthImage;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_normalImage;
    Wolf::ResourceUniqueOwner<Wolf::Sampler> m_sampler;

    std::vector<uint32_t> m_patternsImageUpdateRequested;
    std::mutex m_patternsImageUpdateRequestedLock;
    void patternImagesSanityChecks();
    Wolf::ResourceNonOwner<Wolf::Image> getPatternHeightImage(uint32_t imageIdx);
    Wolf::ResourceNonOwner<Wolf::Image> getPatternNormalImage(uint32_t imageIdx);
    void createDefaultPatternImage();
    Wolf::ResourceUniqueOwner<Wolf::Image> m_defaultPatternHeightImage;

    void createPatternIdxImage();
    Wolf::ResourceUniqueOwner<Wolf::Image> m_patternIdxImage;

    void createDepthCamera();
    bool m_depthCameraCreationRequested = false;
    Wolf::ResourceUniqueOwner<Wolf::OrthographicCamera> m_depthCamera;

    struct UniformBufferData
    {
        glm::mat4 viewProjectionMatrix;

        float yMin;
        float yMax;
        float verticalOffset;
        float globalThickness;

        uint32_t patternImageCount;
        uint32_t materialIdx;
        glm::vec2 padding;
    };
    Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;

    Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
    Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
    void createOrUpdateDescriptorSet();
    void updatePatternInBindless(uint32_t patternIdx);

    Wolf::NullableResourceNonOwner<Entity> m_materialEntity;
    void updateMaterialIdx();
    uint32_t m_materialIdx = 0;
};
