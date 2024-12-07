#pragma once

#include <ModelLoader.h>
#include <PipelineSet.h>

#include "EditorModelInterface.h"
#include "EditorTypesTemplated.h"
#include "ParameterGroupInterface.h"
#include "ResourceManager.h"

class StaticModel : public EditorModelInterface
{
public:
	StaticModel(const glm::mat4& transform, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, const Wolf::ResourceNonOwner<ResourceManager>& resourceManager, 
		const std::function<void(ComponentInterface*)>& requestReloadCallback, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);

	void loadParams(Wolf::JSONReader& jsonReader) override;

	static inline std::string ID = "staticModel";
	std::string getId() const override { return ID; }

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void getMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& outList) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;

	Wolf::AABB getAABB() const override;
	std::string getTypeString() override { return "staticMesh"; }


private:
	inline static const std::string TAB = "Model";
	Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
	Wolf::ResourceNonOwner<ResourceManager> m_resourceManager;
	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;
	ResourceManager::ResourceId m_meshResourceId = ResourceManager::NO_RESOURCE;

	bool m_isWaitingForMeshLoading = false;
	void requestModelLoading();
	void reloadEntity();
	void onSubMeshChanged();
	void subscribeToAllSubMeshes();

	EditorParamString m_loadingPathParam = EditorParamString("Mesh", TAB, "Loading", [this] { requestModelLoading(); }, EditorParamString::ParamStringType::FILE_OBJ);

	class SubMesh : public ParameterGroupInterface, public Notifier
	{
	public:
		SubMesh();
		SubMesh(const SubMesh&) = delete;

		void init(uint32_t idx, const std::string& defaultMaterialName, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);
		void reset();
		void update(const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManager, uint32_t resourceTextureSetIdx);

		void setReloadEntityCallback(const std::function<void()>& callback);

		void getAllParams(std::vector<EditorParamInterface*>& out) const override;
		void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
		bool hasDefaultName() const override { return true; }
		void loadParams(Wolf::JSONReader& jsonReader, const std::string& objectId) override;

		static constexpr uint32_t DEFAULT_MATERIAL_IDX = 0;
		uint32_t getMaterialIdx() const { return m_materialIdx; }

	private:
		std::function<void()> m_reloadEntityCallback;
		std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

		uint32_t m_subMeshIdx = 0;
		std::string m_defaultMaterialName;

		void onShadingModeChanged();
		EditorParamEnum m_shadingMode = EditorParamEnum({ "GGX", "Anisotropic GGX" }, "Shading Mode", TAB, "Material", [this]() { onShadingModeChanged(); });

		void onTextureSetChanged(uint32_t textureSetIdx);
		std::vector<uint32_t> m_textureSetChangedIndices;
		class TextureSet : public ParameterGroupInterface, public Notifier
		{
		public:
			TextureSet();
			TextureSet(const TextureSet&) = delete;

			void setDefaultName(const std::string& value);
			void setGetEntityFromLoadingPathCallback(const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback);

			void getAllParams(std::vector<EditorParamInterface*>& out) const override;
			void getAllVisibleParams(std::vector<EditorParamInterface*>& out) const override;
			bool hasDefaultName() const override;

			static constexpr uint32_t NO_TEXTURE_SET_IDX = -1;
			uint32_t getTextureSetIdx() const;
			float getStrength() const { return m_strength; }

		private:
			inline static const std::string DEFAULT_NAME = "New texture set";
			std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

			void onTextureSetEntityChanged();
			EditorParamString m_textureSetEntityParam = EditorParamString("Texture set entity", TAB, "Texture set", [this]() { onTextureSetEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);
			EditorParamFloat m_strength = EditorParamFloat("Strength", TAB, "Material", 0.01f, 2.0f, [this]() { notifySubscribers(); });
			std::array<EditorParamInterface*, 2> m_allParams =
			{
				&m_textureSetEntityParam,
				&m_strength
			};

			std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_textureSetEntity;
		};
		static constexpr uint32_t MAX_TEXTURE_SETS = 16;
		void onTextureSetAdded();
		EditorParamArray<TextureSet> m_textureSets = EditorParamArray<TextureSet>("Texture sets", TAB, "Material", MAX_TEXTURE_SETS, [this] { onTextureSetAdded(); });

		std::array<EditorParamInterface*, 2> m_allParams =
		{
			&m_shadingMode,
			&m_textureSets
		};

		uint32_t m_materialIdx = DEFAULT_MATERIAL_IDX;

	};
	static constexpr uint32_t MAX_SUB_MESHES = 256;
	EditorParamArray<SubMesh> m_subMeshes = EditorParamArray<SubMesh>("Sub meshes", TAB, "Mesh", MAX_SUB_MESHES, false, true);

	std::array<EditorParamInterface*, 2> m_editorParams =
	{
		&m_loadingPathParam,
		&m_subMeshes
	};

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, StaticModel>> m_defaultPipelineSet;
};

