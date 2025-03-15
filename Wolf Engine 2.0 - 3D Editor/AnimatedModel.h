#pragma once

#include "AnimationHelper.h"
#include "EditorModelInterface.h"
#include "EditorTypesTemplated.h"
#include "ParameterGroupInterface.h"
#include "ResourceManager.h"

class AnimatedModel : public EditorModelInterface
{
public:
	static inline std::string ID = "animatedModel";
	std::string getId() const override { return ID; }

	AnimatedModel(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ResourceManager>& resourceManager, 
		const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, 
		const std::function<void(ComponentInterface*)>& requestReloadCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	bool getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) override;
	bool getMeshesForPhysics(std::vector<EditorPhysicsManager::PhysicsMeshInfo>& outList) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	Wolf::AABB getAABB() const override;
	Wolf::BoundingSphere getBoundingSphere() const override;
	std::string getTypeString() override { return ID; }

	bool requiresInputs() const override { return false; }
	void saveCustom() const override {}

	void getAnimationOptions(std::vector<std::string>& out);
	const std::vector<std::pair<std::string, uint32_t>>& getBoneNamesAndIndices() const { return m_boneNamesAndIndices; }
	glm::vec3 getBonePosition(uint32_t boneIdx) const;
	void setAnimation(uint32_t animationIdx);

private:
	void addBonesToDebug(const Wolf::AnimationData::Bone* bone, DebugRenderingManager& debugRenderingManager);
	void addBoneNamesAndIndices(const Wolf::AnimationData::Bone* bone);

	inline static const std::string TAB = "Model";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<ResourceManager> m_resourceManager;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	ResourceManager::ResourceId m_meshResourceId = ResourceManager::NO_RESOURCE;

	Wolf::AnimationData* findAnimationData(bool& success);

	void updateMaxTimer();
	bool m_updateMaxTimerRequested = false;
	float m_maxTimer = 0.0f;

	uint32_t m_waitingForMeshLoadingFrameCount = 0;
	void requestModelLoading();
	EditorParamString m_loadingPathParam = EditorParamString("Mesh", TAB, "Loading", [this] { requestModelLoading(); }, EditorParamString::ParamStringType::FILE_DAE);

	void onTextureSetEntityChanged();
	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_textureSetEntity;
	bool m_textureSetIdxChanged = false;
	EditorParamString m_textureSetEntityParam = EditorParamString("Texture set entity", TAB, "Texture set", [this]() { onTextureSetEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);
	uint32_t getTextureSetIdx() const;

	class Animation : public ParameterGroupInterface, public Notifier
	{
	public:
		Animation();
		Animation(const Animation&) = delete;

		void setResourceManager(const Wolf::ResourceNonOwner<ResourceManager>& resourceManager) { m_resourceManager.reset(new Wolf::ResourceNonOwner<ResourceManager>(resourceManager)); }

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override;

		uint32_t getResourceId() const { return m_resourceId; }

	protected:
		void onNameChanged() override { notifySubscribers(); }

	private:
		inline static const std::string DEFAULT_NAME = "New animation";
		std::unique_ptr<Wolf::ResourceNonOwner<ResourceManager>> m_resourceManager;

		uint32_t m_resourceId = -1;
		void onFileParamChanged();
		EditorParamString m_fileParam = EditorParamString("File", TAB, "Animation", [this]() { onFileParamChanged(); }, EditorParamString::ParamStringType::FILE_DAE);
		std::array<EditorParamInterface*, 1> m_editorParams =
		{
			&m_fileParam,
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

	uint32_t m_materialIdx = 0;
	std::vector<std::pair<std::string, uint32_t>> m_boneNamesAndIndices;

	std::array<EditorParamInterface*, 9> m_editorParams =
	{
		&m_loadingPathParam,
		&m_textureSetEntityParam,
		&m_animationsParam,
		&m_animationSelectParam,
		&m_showBonesParam,
		&m_highlightBone,
		&m_hideModel,
		&m_forceTimer,
		&m_forceTPoseParam
	};

	/* Rendering */
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, AnimatedModel>> m_defaultPipelineSet;

	uint32_t m_boneCount = 0;
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_bonesBuffer;
	std::vector<BoneInfoGPU> m_bonesInfoGPU;
	std::vector<BoneInfoCPU> m_bonesInfoCPU;
	
	Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, AnimatedModel>> m_descriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
};

