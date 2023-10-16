#include "BuildingModel.h"

#include <fstream>
#include <utility>

#include <DescriptorSetGenerator.h>
#include <JSONReader.h>
#include <ObjLoader.h>

#include "DefaultPipelineInfos.h"
#include "Timer.h"

using namespace Wolf;

DescriptorSetLayoutGenerator Building::g_descriptorSetLayoutGenerator;
VkDescriptorSetLayout Building::g_descriptorSetLayout = nullptr;

BuildingModel::BuildingModel(const glm::mat4& transform, const std::string& filepath, uint32_t materialIdOffset) : ModelInterface(transform)
{
	m_filepath = filepath;
	m_name = "Building";
	m_windowSideSizeInMeter = 2.0f;
	m_windowHeightInMeter = 2.0f;
	m_floorCount = 8;
	m_fullSize = glm::vec3(10.0f, m_windowHeightInMeter * m_floorCount, 20.0f);

	if (std::filesystem::exists(m_filepath))
	{
		JSONReader jsonReader(m_filepath);

		m_fullSize.x = jsonReader.getRoot()->getPropertyFloat("fullSizeX");
		m_fullSize.z = jsonReader.getRoot()->getPropertyFloat("fullSizeZ");

		const std::string& meshLoadingPath = jsonReader.getRoot()->getPropertyString("windowMeshLoadingPath");
		if (meshLoadingPath.empty())
			setDefaultWindowMesh(materialIdOffset);
		else
			loadWindowMesh(meshLoadingPath, "", materialIdOffset);

		m_windowSideSizeInMeter = jsonReader.getRoot()->getPropertyFloat("windowSideSizeInMeter");
		m_windowHeightInMeter = jsonReader.getRoot()->getPropertyFloat("windowHeightInMeter");
		m_floorCount = jsonReader.getRoot()->getPropertyFloat("floorCount");

		m_fullSize.y = m_windowHeightInMeter * m_floorCount;
	}
	else
	{
		setDefaultWindowMesh(materialIdOffset);

		save();
	}

	// Model descriptor set
	m_descriptorSet.reset(new DescriptorSet(DefaultPipeline::g_descriptorSetLayout, UpdateRate::NEVER));

	DescriptorSetGenerator descriptorSetGenerator(DefaultPipeline::g_descriptorSetLayoutGenerator.getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *m_matricesUniformBuffer);
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	// Building descriptor set
	m_windowsInfoUniformBuffer.reset(new Buffer(sizeof(Building::MeshInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, UpdateRate::NEVER));

	m_windowsDescriptorSet.reset(new DescriptorSet(Building::g_descriptorSetLayout, UpdateRate::NEVER));
	DescriptorSetGenerator buildingDescriptorSetGenerator(Building::g_descriptorSetLayoutGenerator.getDescriptorLayouts());
	buildingDescriptorSetGenerator.setBuffer(0, *m_windowsInfoUniformBuffer);
	m_windowsDescriptorSet->update(buildingDescriptorSetGenerator.getDescriptorSetCreateInfo());

	rebuildRenderingInfos();
}

void BuildingModel::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const
{
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, m_descriptorSet->getDescriptorSet(), 0, nullptr);

	// Windows
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, m_windowsDescriptorSet->getDescriptorSet(), 0, nullptr);

	const VkDeviceSize offsets[1] = { 0 };
	const VkBuffer windowsInstanceBuffer = m_windowInstanceBuffer->getBuffer();
	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &windowsInstanceBuffer, offsets);
	m_windowMesh.mesh->draw(commandBuffer, m_windowInstanceCount);
}

void BuildingModel::getImages(std::vector<Wolf::Image*>& images)
{
	for (std::unique_ptr<Image>& image : m_windowMesh.images)
	{
		images.push_back(image.get());
	}
}

void BuildingModel::setBuildingSizeX(float value)
{
	m_fullSize.x = value;
	rebuildRenderingInfos();
}

void BuildingModel::setBuildingSizeZ(float value)
{
	m_fullSize.z = value;
	rebuildRenderingInfos();
}

void BuildingModel::setWindowSideSize(float value)
{
	m_windowSideSizeInMeter = value;
	rebuildRenderingInfos();
}

void BuildingModel::setFloorCount(uint32_t value)
{
	m_floorCount = value;
	m_fullSize.y = m_windowHeightInMeter * m_floorCount;
	rebuildRenderingInfos();
}

void BuildingModel::loadWindowMesh(const std::string& filename, const std::string& materialFolder, uint32_t materialIdOffset)
{
	Timer timer(filename + " loading");

	ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = filename;
	modelLoadingInfo.mtlFolder = materialFolder;
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.loadMaterials = true;
	modelLoadingInfo.materialIdOffset = materialIdOffset;
	ModelData windowMeshData;
	ObjLoader::loadObject(windowMeshData, modelLoadingInfo);

	m_windowMesh.loadingPath = filename;
	m_windowMesh.mesh = std::move(windowMeshData.mesh);
	m_windowMesh.images.resize(windowMeshData.images.size());
	for (uint32_t i = 0; i < windowMeshData.images.size(); ++i)
	{
		m_windowMesh.images[i] = std::move(windowMeshData.images[i]);
	}
	const glm::vec3 meshSize = m_windowMesh.mesh->getAABB().getSize();
	m_windowMesh.meshSizeInMeter = glm::vec2(meshSize.x, meshSize.y);

	const glm::vec3 meshCenter = m_windowMesh.mesh->getAABB().getCenter();
	m_windowMesh.center = glm::vec2(meshCenter.x, meshCenter.y);

	if (m_windowsInfoUniformBuffer)
		rebuildRenderingInfos();
}

void BuildingModel::save() const
{
	std::ofstream outputFile(m_filepath);

	// Header comments
	const time_t now = time(nullptr);
	tm newTime;
	localtime_s(&newTime, &now);
	outputFile << "// Save scene: " << newTime.tm_mday << "/" << 1 + newTime.tm_mon << "/" << 1900 + newTime.tm_year << " " << newTime.tm_hour << ":" << newTime.tm_min << "\n\n";

	outputFile << "{\n";

	outputFile << "\t\"fullSizeX\":" << m_fullSize.x << ",\n";
	outputFile << "\t\"fullSizeZ\":" << m_fullSize.z << ",\n";

	// Windows
	outputFile << "\t\"windowMeshLoadingPath\":\"" << m_windowMesh.loadingPath << "\",\n";
	outputFile << "\t\"windowSideSizeInMeter\":" << m_windowSideSizeInMeter << ",\n";
	outputFile << "\t\"windowHeightInMeter\":" << m_windowHeightInMeter << ",\n";

	// Floors
	outputFile << "\t\"floorCount\":" << m_floorCount << "\n";

	outputFile << "}\n";

	outputFile.close();
}

void BuildingModel::setDefaultWindowMesh(uint32_t materialIdOffset)
{
	const std::vector<Vertex3D> vertices =
	{
		{ glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), materialIdOffset }, // top left
		{ glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f), materialIdOffset }, // top right
		{ glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f), materialIdOffset }, // bot left
		{ glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f),glm::vec2(1.0f, 1.0f), materialIdOffset } // bot right
	};
	const std::vector<uint32_t> indices =
	{
		0, 2, 1,
		2, 3, 1
	};
	m_windowMesh.mesh.reset(new Mesh(vertices, indices));
	m_windowMesh.meshSizeInMeter = glm::vec2(2.0f, 2.0f);
	m_windowMesh.center = glm::vec2(0.0f, 0.0f);

	m_windowMesh.images.resize(5);
	constexpr uint32_t DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE = 32;
	auto createImage = [](std::unique_ptr<Image>& image)
		{
			CreateImageInfo createImageInfo;
			createImageInfo.extent = { DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE, DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE, 1 };
			createImageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
			createImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			createImageInfo.mipLevelCount = 1;
			createImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			image.reset(new Image(createImageInfo));
		};

	struct Pixel
	{
		uint8_t r, g, b, a;
	};

	// Albedo
	{
		std::unique_ptr<Image>& image = m_windowMesh.images[0];
		createImage(image);

		std::array<Pixel, DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE* DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE> pixels;
		for (uint32_t pixelX = 0; pixelX < DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE; ++pixelX)
		{
			for (uint32_t pixelY = 0; pixelY < DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE; ++pixelY)
			{
				if (pixelX == 0 || pixelX == DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE - 1 || pixelY == 0 || pixelY == DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE - 1)
					pixels[pixelX + DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE * pixelY] = { 255, 255, 255, 255 };
				else
					pixels[pixelX + DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE * pixelY] = { 255, 0, 120, 255 };
			}
		}
		image->copyCPUBuffer(reinterpret_cast<const unsigned char*>(&pixels), Image::SampledInFragmentShader());
	}

	// Normal
	{
		std::unique_ptr<Image>& image = m_windowMesh.images[1];
		createImage(image);
	}

	// Roughness
	{
		std::unique_ptr<Image>& image = m_windowMesh.images[2];
		createImage(image);
	}

	// Metalness
	{
		std::unique_ptr<Image>& image = m_windowMesh.images[3];
		createImage(image);
	}

	// AO
	{
		std::unique_ptr<Image>& image = m_windowMesh.images[4];
		createImage(image);
	}
}

float BuildingModel::computeFloorSize() const
{
	return m_fullSize.y / static_cast<float>(m_floorCount);
}

float BuildingModel::computeWindowCountOnSide(const glm::vec3& sideDir) const
{
	return glm::length(m_fullSize * sideDir) / m_windowSideSizeInMeter;
}

void BuildingModel::rebuildRenderingInfos()
{
	std::vector<InstanceData> windowInstances;

	for (uint32_t floorIdx = 0; floorIdx < m_floorCount; ++floorIdx)
	{
		const float floorHeight = computeFloorSize() * floorIdx;

		struct SideInfo
		{
			glm::vec3 normal;
			glm::vec3 firstWindowPos;
			glm::vec3 offsetBetweenWindowCenters;
		};
		const std::array<SideInfo, 4> sideInfos =
		{
			SideInfo {
				glm::vec3(1.0f, 0.0f,  0.0f),
				glm::vec3(m_fullSize.x / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_fullSize.z / 2.0f + m_windowSideSizeInMeter / 2.0f),
				glm::vec3(0.0f, 0.0f, m_windowSideSizeInMeter)
			},
			SideInfo {
				glm::vec3(-1.0f, 0.0f,  0.0f),
				glm::vec3(-m_fullSize.x / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_fullSize.z / 2.0f + m_windowSideSizeInMeter / 2.0f),
				glm::vec3(0.0f, 0.0f, m_windowSideSizeInMeter)
			},
			SideInfo {
				glm::vec3(0.0f, 0.0f,  1.0f),
				glm::vec3(-m_fullSize.x / 2.0f + m_windowSideSizeInMeter / 2.0f, computeFloorSize() / 2.0f + floorHeight, m_fullSize.z / 2.0f),
				glm::vec3(m_windowSideSizeInMeter, 0.0f, 0.0f)
			},
			SideInfo {
				glm::vec3(0.0f, 0.0f, -1.0f),
				glm::vec3(-m_fullSize.x / 2.0f + m_windowSideSizeInMeter / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_fullSize.z / 2.0f),
				glm::vec3(m_windowSideSizeInMeter, 0.0f, 0.0f)
			}
		};
		for (const SideInfo& sideInfo : sideInfos)
		{
			for (uint32_t windowIdx = 0, end = static_cast<uint32_t>(computeWindowCountOnSide(glm::normalize(sideInfo.offsetBetweenWindowCenters))); windowIdx < end; ++windowIdx)
			{
				const glm::vec3 windowCenterPos = sideInfo.firstWindowPos + static_cast<float>(windowIdx) * sideInfo.offsetBetweenWindowCenters;
				windowInstances.push_back({ windowCenterPos, sideInfo.normal });
			}
		}
	}

	m_windowInstanceBuffer.reset(new Buffer(windowInstances.size() * sizeof(InstanceData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, UpdateRate::NEVER));
	m_windowInstanceBuffer->transferCPUMemoryWithStagingBuffer(windowInstances.data(), sizeof(InstanceData) * windowInstances.size());
	m_windowInstanceCount = windowInstances.size();

	Building::MeshInfo windowsInfo;
	windowsInfo.scale = glm::vec2(m_windowSideSizeInMeter / m_windowMesh.meshSizeInMeter.x, m_windowHeightInMeter / m_windowMesh.meshSizeInMeter.y);
	windowsInfo.offset = -m_windowMesh.center;
	m_windowsInfoUniformBuffer->transferCPUMemory(&windowsInfo, sizeof(windowsInfo), 0 /* srcOffet */);
}
