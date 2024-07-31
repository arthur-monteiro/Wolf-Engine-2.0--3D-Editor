#pragma once

#include <AABB.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Mesh.h>
#include <PipelineSet.h>

#include "EditorModelInterface.h"
#include "ModelLoader.h"
#include "Notifier.h"

class BuildingModel : public EditorModelInterface
{
public:
	struct InstanceData
	{
		glm::vec3 offset;
		glm::vec3 newNormal;

		static void getBindingDescription(VkVertexInputBindingDescription& bindingDescription, uint32_t binding)
		{
			bindingDescription.binding = binding;
			bindingDescription.stride = sizeof(InstanceData);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		}

		static void getAttributeDescriptions(std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, uint32_t binding, uint32_t startLocation)
		{
			const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
			attributeDescriptions.resize(attributeDescriptionCountBefore + 2);

			attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 0].location = startLocation;
			attributeDescriptions[attributeDescriptionCountBefore + 0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(InstanceData, offset);

			attributeDescriptions[attributeDescriptionCountBefore + 1].binding = binding;
			attributeDescriptions[attributeDescriptionCountBefore + 1].location = startLocation + 1;
			attributeDescriptions[attributeDescriptionCountBefore + 1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[attributeDescriptionCountBefore + 1].offset = offsetof(InstanceData, newNormal);
		}
	};

	BuildingModel(const glm::mat4& transform, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManage);

	static inline std::string ID = "buildingModel";
	std::string getId() const override { return ID; }

	void loadParams(Wolf::JSONReader& jsonReader) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void getMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& outList) override;
	void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount = 2) override;
	
	Wolf::AABB getAABB() const override { return Wolf::AABB(); }

	enum class PieceType { WINDOW, WALL };

	std::string getTypeString() override { return "building"; }
	const std::string& getWindowMeshLoadingPath(uint32_t meshIdx) const { return m_window.getMeshWithMaterials().getLoadingPath(); }

private:
	struct MeshInfo
	{
		glm::vec2 scale;
		glm::vec2 offset;
	};

	class MeshWithMaterials : public Notifier
	{
	public:
		MeshWithMaterials(const std::string& category, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManage) :
			m_materialsGPUManager(materialsGPUManage), m_loadingPathParam("Mesh", "Building", category, [this] { requestMeshLoading(); }, EditorParamString::ParamStringType::FILE_OBJ) {}

		void updateBeforeFrame();
		void loadMesh();
		void loadDefaultMesh(const glm::vec3& color);

		void setLoadingPath(const std::string& loadingPath) { m_loadingPathParam = loadingPath; }

		const std::string& getLoadingPath() const { return m_loadingPathParam; }
		Wolf::ResourceNonOwner<Wolf::Mesh> getMesh() { return m_modelData.mesh.createNonOwnerResource(); }
		const glm::vec2& getSizeInMeter() const { return m_sizeInMeter; }
		const glm::vec2& getCenter() const { return m_center; }

		EditorParamString* getLoadingPathParam() { return &m_loadingPathParam; }

	private:
		Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> m_materialsGPUManager;
		void addMaterialsToGPU() const;

		EditorParamString m_loadingPathParam;
		Wolf::ModelData m_modelData;
		glm::vec2 m_sizeInMeter;
		glm::vec2 m_center;

		void requestMeshLoading();
		bool m_meshLoadingRequested = false;
	};

	class BuildingPiece : public Notifier
	{
	public:
		BuildingPiece(const std::string& name, const std::function<void()>& callbackValueChanged, const Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager>& materialsGPUManage) :
			m_sideSizeInMeterParam("Side size in meter", "Building", name, 0.1f, 5.0f, callbackValueChanged),
			m_meshWithMaterials(name, materialsGPUManage)
		{
			m_sideSizeInMeterParam = 2.0f;
			m_meshWithMaterials.subscribe(this, [this] { notifySubscribers(); });
		}

		MeshWithMaterials& getMeshWithMaterials() { return m_meshWithMaterials; }
		const MeshWithMaterials& getMeshWithMaterials() const { return m_meshWithMaterials; }
		EditorParamFloat* getSideSizeInMeterParam() { return &m_sideSizeInMeterParam; }
		const EditorParamFloat* getSideSizeInMeterParam() const { return &m_sideSizeInMeterParam; }
		float getSideSizeInMeter() const { return m_sideSizeInMeterParam; }
		float getHeightInMeter() const { return m_heightInMeter; }
		std::unique_ptr<Wolf::Buffer>& getInfoUniformBuffer() { return m_infoUniformBuffer; }
		Wolf::ResourceUniqueOwner<Wolf::DescriptorSet>& getDescriptorSet() { return m_descriptorSet; }
		std::unique_ptr<Wolf::Buffer>& getInstanceBuffer() { return m_instanceBuffer; }
		const std::unique_ptr<Wolf::Buffer>& getInstanceBuffer() const { return m_instanceBuffer; }
		uint32_t getInstanceCount() const { return m_instanceCount; }

		void setHeightInMeter(float value) { m_heightInMeter = value; }
		void setInstanceCount(uint32_t value) { m_instanceCount = value; }

	private:
		EditorParamFloat m_sideSizeInMeterParam;
		float m_heightInMeter = 2.0f;
		MeshWithMaterials m_meshWithMaterials;

		Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
		std::unique_ptr<Wolf::Buffer> m_instanceBuffer;
		uint32_t m_instanceCount = 0;
		std::unique_ptr<Wolf::Buffer> m_infoUniformBuffer;
	};

	float computeFloorSize() const;
	float computeWindowCountOnSide(const glm::vec3& sideDir) const;
	void rebuildRenderingInfos();
	
	float m_fullSizeY;
	bool m_needRebuild = false;

	// Windows
	BuildingPiece m_window;

	// Walls
	BuildingPiece m_wall;

	// Floors
	float m_floorHeightInMeter;

	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::PipelineSet, BuildingModel>> m_defaultPipelineSet;
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayoutGenerator, BuildingModel>> m_buildingDescriptorSetLayoutGenerator;
	std::unique_ptr<Wolf::LazyInitSharedResource<Wolf::DescriptorSetLayout, BuildingModel>> m_buildingDescriptorSetLayout;

	EditorParamVector2 m_sizeXZParam = EditorParamVector2("Building size", "Building", "General", 1.0f, 150.0f, [this] { m_needRebuild = true; });
	EditorParamUInt m_floorCountParam = EditorParamUInt("Floor count", "Building", "Floors", 1, 50, [this]
	{
		m_fullSizeY = m_floorHeightInMeter * static_cast<float>(m_floorCountParam);
		m_needRebuild = true;
	});

	std::array<EditorParamInterface*, 6> m_buildingParams =
	{
		&m_sizeXZParam,
		&m_floorCountParam,
		m_window.getSideSizeInMeterParam(),
		m_window.getMeshWithMaterials().getLoadingPathParam(),
		m_wall.getSideSizeInMeterParam(),
		m_wall.getMeshWithMaterials().getLoadingPathParam()
	};
};