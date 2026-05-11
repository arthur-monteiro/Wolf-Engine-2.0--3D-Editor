#pragma once

#include "AnimationHelper.h"
#include "EditorModelInterface.h"
#include "EditorTypesTemplated.h"
#include "ParameterGroupInterface.h"
#include "AssetManager.h"

class AnimatedMesh : public EditorModelInterface
{
public:
	static inline std::string ID = "animatedMesh";
	std::string getId() const override { return ID; }

	AnimatedMesh(const Wolf::ResourceNonOwner<AssetManager>& resourceManager, const std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback,
		const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<void(ComponentInterface*)>& requestReloadCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	bool getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) override;
	bool getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	Wolf::AABB getAABB() const override;
	Wolf::BoundingSphere getBoundingSphere() const override;

	void saveCustom() const override {}

	void setInfoFromParent(AssetId assetId);

	void getAnimationOptions(std::vector<std::string>& out);
	const std::vector<std::pair<std::string, uint32_t>>& getBoneNamesAndIndices() const { return m_boneNamesAndIndices; }
	glm::vec3 getBonePosition(uint32_t boneIdx) const;
	void setAnimation(uint32_t animationIdx);

private:
	void addBonesToDebug(const AnimationData::Bone* bone, DebugRenderingManager& debugRenderingManager);
	void addBoneNamesAndIndices(const AnimationData::Bone* bone);

	inline static const std::string TAB = "Mesh";
	Wolf::ResourceNonOwner<AssetManager> m_assetManager;
	std::function<Wolf::NullableResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	AssetId m_meshAssetId = NO_ASSET;

	Wolf::NullableResourceNonOwner<AnimationData> findAnimationData(bool& success);

	void updateMaxTimer();
	bool m_updateMaxTimerRequested = false;
	float m_maxTimer = 0.0f;

	uint32_t m_waitingForMeshLoadingFrameCount = 0;
	void onMeshChanged();
	EditorParamString m_meshAssetParam = EditorParamString("Mesh", TAB, "Loading", [this] { onMeshChanged(); }, EditorParamString::ParamStringType::ASSET);

	void onMaterialAssetChanged();
	uint32_t m_materialGPUIdx = 0;
	EditorParamString m_materialAsset = EditorParamString("Material", TAB, "Material", [this]() { onMaterialAssetChanged(); }, EditorParamString::ParamStringType::ASSET);

	class Animation : public ParameterGroupInterface, public Notifier
	{
	public:
		Animation();
		Animation(const Animation&) = delete;

		void setAssetManager(const Wolf::ResourceNonOwner<AssetManager>& assetManager) { m_assetManager = assetManager; }

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override;

		uint32_t getAssetId() const { return m_assetId; }

	protected:
		void onNameChanged() override { notifySubscribers(); }

	private:
		inline static const std::string DEFAULT_NAME = "New animation";
		Wolf::NullableResourceNonOwner<AssetManager> m_assetManager;

		uint32_t m_assetId = -1;
		void onAnimationParamChanged();
		EditorParamString m_animationAssetParam = EditorParamString("File", TAB, "Animation", [this]() { onAnimationParamChanged(); }, EditorParamString::ParamStringType::ASSET);
		std::array<EditorParamInterface*, 1> m_editorParams =
		{
			&m_animationAssetParam,
		};
	};

	void onAnimationAdded();
	static constexpr uint32_t MAX_ANIMATION = 8;
	EditorParamArray<Animation> m_animationsParam = EditorParamArray<Animation>("Animations", TAB, "Animation", MAX_ANIMATION, [this] { onAnimationAdded(); });

	void updateAnimationsOptions();
	EditorParamEnum m_animationSelectParam = EditorParamEnum({ "Default" }, "Animation", TAB, "Debug", [this]() { m_updateMaxTimerRequested = true; });
	EditorParamBool m_showBonesParam = EditorParamBool("Show bones", TAB, "Debug");
	EditorParamEnum m_highlightBone = EditorParamEnum({}, "Highlight bone", TAB, "Debug");
	EditorParamBool m_hideModel = EditorParamBool("Hide model", TAB, "Debug", [this]() { notifySubscribers(); });
	EditorParamFloat m_forceTimer = EditorParamFloat("Force timer", TAB, "Debug", 0.0f, 1.0f, true);
	EditorParamBool m_forceTPoseParam = EditorParamBool("Force T-Pose", TAB, "Debug");

	std::vector<std::pair<std::string, uint32_t>> m_boneNamesAndIndices;

	std::array<EditorParamInterface*, 9> m_editorParams =
	{
		&m_meshAssetParam,
		&m_materialAsset,
		&m_animationsParam,
		&m_animationSelectParam,
		&m_showBonesParam,
		&m_highlightBone,
		&m_hideModel,
		&m_forceTimer,
		&m_forceTPoseParam
	};

	/* Rendering */
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, AnimatedMesh>> m_defaultPipelineSet;

	uint32_t m_boneCount = 0;
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_bonesBuffer;
	std::vector<BoneInfoGPU> m_bonesInfoGPU;
	std::vector<BoneInfoCPU> m_bonesInfoCPU;
	
	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, AnimatedMesh>> m_descriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
};

