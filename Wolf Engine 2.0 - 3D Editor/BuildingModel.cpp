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
	m_floorCount = 8;
	m_floorHeightInMeter = 2.0f;
	m_fullSize = glm::vec3(10.0f, m_floorHeightInMeter * m_floorCount, 20.0f);

	if (std::filesystem::exists(m_filepath))
	{
		JSONReader jsonReader(m_filepath);

		m_fullSize.x = jsonReader.getRoot()->getPropertyFloat("fullSizeX");
		m_fullSize.z = jsonReader.getRoot()->getPropertyFloat("fullSizeZ");

		const std::string& meshLoadingPath = jsonReader.getRoot()->getPropertyString("windowMeshLoadingPath");
		if (meshLoadingPath.empty())
			setDefaultWindowMesh(materialIdOffset);
		else
			loadPieceMesh(meshLoadingPath, "", materialIdOffset, PieceType::WINDOW);

		setDefaultWallMesh(materialIdOffset + 1);

		m_window.sideSizeInMeter = jsonReader.getRoot()->getPropertyFloat("windowSideSizeInMeter");
		m_window.heightInMeter = jsonReader.getRoot()->getPropertyFloat("windowHeightInMeter");
		m_floorCount = jsonReader.getRoot()->getPropertyFloat("floorCount");

		m_fullSize.y = m_floorHeightInMeter * m_floorCount;
	}
	else
	{
		setDefaultWindowMesh(materialIdOffset);
		setDefaultWallMesh(materialIdOffset + 1);

		save();
	}

	// Model descriptor set
	m_descriptorSet.reset(new DescriptorSet(DefaultPipeline::g_descriptorSetLayout, UpdateRate::NEVER));

	DescriptorSetGenerator descriptorSetGenerator(DefaultPipeline::g_descriptorSetLayoutGenerator.getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *m_matricesUniformBuffer);
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	// Building descriptor set
	auto initBufferAndDescriptorSet = [](BuildingPiece& piece)
	{
		piece.infoUniformBuffer.reset(new Buffer(sizeof(Building::MeshInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, UpdateRate::NEVER));
		piece.descriptorSet.reset(new DescriptorSet(Building::g_descriptorSetLayout, UpdateRate::NEVER));
		DescriptorSetGenerator buildingDescriptorSetGenerator(Building::g_descriptorSetLayoutGenerator.getDescriptorLayouts());
		buildingDescriptorSetGenerator.setBuffer(0, *piece.infoUniformBuffer);
		piece.descriptorSet->update(buildingDescriptorSetGenerator.getDescriptorSetCreateInfo());
	};

	initBufferAndDescriptorSet(m_window);
	initBufferAndDescriptorSet(m_wall);

	rebuildRenderingInfos();
}

void BuildingModel::updateGraphic(const CameraInterface& camera)
{
	ModelInterface::updateGraphic(camera);

	if (m_needRebuild)
	{
		rebuildRenderingInfos();
	}
}

void BuildingModel::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const
{
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, m_descriptorSet->getDescriptorSet(), 0, nullptr);

	auto drawPiece = [&](const BuildingPiece& piece)
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 3, 1, piece.descriptorSet->getDescriptorSet(), 0, nullptr);
		const VkDeviceSize offsets[1] = { 0 };
		const VkBuffer windowsInstanceBuffer = piece.instanceBuffer->getBuffer();
		vkCmdBindVertexBuffers(commandBuffer, 1, 1, &windowsInstanceBuffer, offsets);
		piece.mesh.mesh->draw(commandBuffer, piece.instanceCount);
	};

	drawPiece(m_window);
	drawPiece(m_wall);
}

void BuildingModel::getAllImages(std::vector<Wolf::Image*>& images)
{
	getImagesForPiece(images, PieceType::WINDOW);
	getImagesForPiece(images, PieceType::WALL);
}

void BuildingModel::getImagesForPiece(std::vector<Wolf::Image*>& images, PieceType pieceType)
{
	BuildingPiece* buildingPiece = nullptr;
	switch (pieceType)
	{
	case PieceType::WINDOW:
		buildingPiece = &m_window;
		break;
	case PieceType::WALL:
		buildingPiece = &m_wall;
		break;
	default:
		Debug::sendError("Not handled piece type");
	}

	for (std::unique_ptr<Image>& image : buildingPiece->mesh.images)
		images.push_back(image.get());
}

void BuildingModel::setBuildingSizeX(float value)
{
	m_fullSize.x = value;
	m_needRebuild = true;
}

void BuildingModel::setBuildingSizeZ(float value)
{
	m_fullSize.z = value;
	m_needRebuild = true;
}

void BuildingModel::setWindowSideSize(float value)
{
	m_window.sideSizeInMeter = value;
	m_needRebuild = true;
}

void BuildingModel::setFloorCount(uint32_t value)
{
	m_floorCount = value;
	m_fullSize.y = m_floorHeightInMeter * m_floorCount;
	m_needRebuild = true;
}

void BuildingModel::loadPieceMesh(const std::string& filename, const std::string& materialFolder, uint32_t materialIdOffset, PieceType pieceType)
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

	BuildingPiece* buildingPiece = nullptr;
	switch (pieceType)
	{
		case PieceType::WINDOW:
			buildingPiece = &m_window;
			break;
		case PieceType::WALL:
			buildingPiece = &m_wall;
			break;
		default:
			Debug::sendError("Not handled piece type");
	}

	buildingPiece->mesh.loadingPath = filename;
	buildingPiece->mesh.mesh = std::move(windowMeshData.mesh);
	buildingPiece->mesh.images.resize(windowMeshData.images.size());
	for (uint32_t i = 0; i < windowMeshData.images.size(); ++i)
	{
		buildingPiece->mesh.images[i] = std::move(windowMeshData.images[i]);
	}
	const glm::vec3 meshSize = buildingPiece->mesh.mesh->getAABB().getSize();
	buildingPiece->mesh.meshSizeInMeter = glm::vec2(meshSize.x, meshSize.y);

	const glm::vec3 meshCenter = buildingPiece->mesh.mesh->getAABB().getCenter();
	buildingPiece->mesh.center = glm::vec2(meshCenter.x, meshCenter.y);

	if (buildingPiece->infoUniformBuffer)
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

	auto savePiece = [&](const BuildingPiece& piece, const std::string& name)
	{
		outputFile << "\t\"" + name + "MeshLoadingPath\":\"" << piece.mesh.loadingPath << "\",\n";
		outputFile << "\t\"" + name + "SideSizeInMeter\":" << piece.sideSizeInMeter << ",\n";
		outputFile << "\t\"" + name + "HeightInMeter\":" << piece.heightInMeter << ",\n";
	};
	savePiece(m_window, "window");
	savePiece(m_wall, "wall");

	// Floors
	outputFile << "\t\"floorCount\":" << m_floorCount << "\n";

	outputFile << "}\n";

	outputFile.close();
}

void BuildingModel::setDefaultMesh(MeshWithMaterials& output, const glm::vec3& color, uint32_t materialIdOffset)
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
	output.mesh.reset(new Mesh(vertices, indices));
	output.meshSizeInMeter = glm::vec2(2.0f, 2.0f);
	output.center = glm::vec2(0.0f, 0.0f);

	output.images.resize(5);
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
		std::unique_ptr<Image>& image = output.images[0];
		createImage(image);

		std::array<Pixel, DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE* DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE> pixels;
		for (uint32_t pixelX = 0; pixelX < DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE; ++pixelX)
		{
			for (uint32_t pixelY = 0; pixelY < DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE; ++pixelY)
			{
				if (pixelX == 0 || pixelX == DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE - 1 || pixelY == 0 || pixelY == DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE - 1)
					pixels[pixelX + DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE * pixelY] = { 255, 255, 255, 255 };
				else
					pixels[pixelX + DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE * pixelY] = { static_cast<uint8_t>(color.x * 255.0f), static_cast<uint8_t>(color.y * 255.0f), static_cast<uint8_t>(color.z * 255.0f), 255 };
			}
		}
		image->copyCPUBuffer(reinterpret_cast<const unsigned char*>(&pixels), Image::SampledInFragmentShader());
	}

	// Normal
	{
		std::unique_ptr<Image>& image = output.images[1];
		createImage(image);
	}

	// Roughness
	{
		std::unique_ptr<Image>& image = output.images[2];
		createImage(image);
	}

	// Metalness
	{
		std::unique_ptr<Image>& image = output.images[3];
		createImage(image);
	}

	// AO
	{
		std::unique_ptr<Image>& image = output.images[4];
		createImage(image);
	}
}

void BuildingModel::setDefaultWindowMesh(uint32_t materialIdOffset)
{
	setDefaultMesh(m_window.mesh, glm::vec3(1.0f, 0.0f, 0.4f), materialIdOffset);
}

void BuildingModel::setDefaultWallMesh(uint32_t materialIdOffset)
{
	setDefaultMesh(m_wall.mesh, glm::vec3(0.5f, 0.25f, 0.0f), materialIdOffset);
}

float BuildingModel::computeFloorSize() const
{
	return m_fullSize.y / static_cast<float>(m_floorCount);
}

float BuildingModel::computeWindowCountOnSide(const glm::vec3& sideDir) const
{
	return std::ceil(glm::length(m_fullSize * sideDir) / (m_window.sideSizeInMeter + m_wall.sideSizeInMeter));
}

void BuildingModel::rebuildRenderingInfos()
{
	std::vector<InstanceData> windowInstances;
	std::vector<InstanceData> wallInstances;

	for (uint32_t floorIdx = 0; floorIdx < m_floorCount; ++floorIdx)
	{
		const float floorHeight = computeFloorSize() * floorIdx;

		struct SideInfo
		{
			glm::vec3 normal;
			glm::vec3 firstWindowPos;
			glm::vec3 offsetScaleBetweenWindowCenters;
		};
		const std::array<SideInfo, 4> sideInfos =
		{
			SideInfo {
				glm::vec3(1.0f, 0.0f,  0.0f),
				glm::vec3(m_fullSize.x / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_fullSize.z / 2.0f + m_window.sideSizeInMeter / 2.0f),
				glm::vec3(0.0f, 0.0f, 1.0f)
			},
			SideInfo {
				glm::vec3(-1.0f, 0.0f,  0.0f),
				glm::vec3(-m_fullSize.x / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_fullSize.z / 2.0f + m_window.sideSizeInMeter / 2.0f),
				glm::vec3(0.0f, 0.0f, 1.0f)
			},
			SideInfo {
				glm::vec3(0.0f, 0.0f,  1.0f),
				glm::vec3(-m_fullSize.x / 2.0f + m_window.sideSizeInMeter / 2.0f, computeFloorSize() / 2.0f + floorHeight, m_fullSize.z / 2.0f),
				glm::vec3(1.0f, 0.0f, 0.0f)
			},
			SideInfo {
				glm::vec3(0.0f, 0.0f, -1.0f),
				glm::vec3(-m_fullSize.x / 2.0f + m_window.sideSizeInMeter / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_fullSize.z / 2.0f),
				glm::vec3(1.0f, 0.0f, 0.0f)
			}
		};
		for (const SideInfo& sideInfo : sideInfos)
		{
			for (uint32_t windowIdx = 0, end = static_cast<uint32_t>(computeWindowCountOnSide(glm::normalize(sideInfo.offsetScaleBetweenWindowCenters))); windowIdx < end; ++windowIdx)
			{
				const float spaceBetweenWindows = m_window.sideSizeInMeter + m_wall.sideSizeInMeter;

				const glm::vec3 windowCenterPos = sideInfo.firstWindowPos + static_cast<float>(windowIdx) * (sideInfo.offsetScaleBetweenWindowCenters * spaceBetweenWindows);
				windowInstances.push_back({ windowCenterPos, sideInfo.normal });

				if (glm::length(m_fullSize * sideInfo.offsetScaleBetweenWindowCenters) / (m_window.sideSizeInMeter + m_wall.sideSizeInMeter) != computeWindowCountOnSide(glm::normalize(sideInfo.offsetScaleBetweenWindowCenters)) && windowIdx == end - 1)
					continue;

				const glm::vec3 wallCenterPos = sideInfo.firstWindowPos + static_cast<float>(windowIdx) * (sideInfo.offsetScaleBetweenWindowCenters * spaceBetweenWindows) + (sideInfo.offsetScaleBetweenWindowCenters * m_window.sideSizeInMeter);
				wallInstances.push_back({ wallCenterPos, sideInfo.normal });
			}
		}
	}

	auto updateBuffers = [](BuildingPiece& piece, const std::vector<InstanceData>& data)
	{
		piece.instanceBuffer.reset(new Buffer(data.size() * sizeof(InstanceData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, UpdateRate::NEVER));
		piece.instanceBuffer->transferCPUMemoryWithStagingBuffer(data.data(), sizeof(InstanceData) * data.size());
		piece.instanceCount = data.size();

		Building::MeshInfo windowsInfo;
		windowsInfo.scale = glm::vec2(piece.sideSizeInMeter / piece.mesh.meshSizeInMeter.x, piece.heightInMeter / piece.mesh.meshSizeInMeter.y);
		windowsInfo.offset = -piece.mesh.center;
		piece.infoUniformBuffer->transferCPUMemory(&windowsInfo, sizeof(windowsInfo), 0 /* srcOffet */);
	};
	updateBuffers(m_window, windowInstances);
	updateBuffers(m_wall, wallInstances);

	m_needRebuild = false;
}
