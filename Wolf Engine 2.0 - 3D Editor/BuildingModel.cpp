#include "BuildingModel.h"

#include <fstream>
#include <utility>

#include <DescriptorSetGenerator.h>
#include <JSONReader.h>
#include <ModelLoader.h>

#include "CommonDescriptorLayouts.h"
#include "EditorParamsHelper.h"
#include "Entity.h"
#include "Timer.h"

using namespace Wolf;

BuildingModel::BuildingModel(const glm::mat4& transform, const ResourceNonOwner<MaterialsGPUManager>& materialsGPUManage)
	: EditorModelInterface(transform),
	  m_window("Window", [this] { m_needRebuild = true; }, materialsGPUManage),
	  m_wall("Wall", [this] { m_needRebuild = true; }, materialsGPUManage)
{
	m_floorCountParam = 8;
	m_floorHeightInMeter = 2.0f;
	m_sizeXZParam = glm::vec2(10.0f, 20.0f);
	m_fullSizeY = m_floorHeightInMeter * static_cast<uint32_t>(m_floorCountParam);

	m_buildingDescriptorSetLayoutGenerator.reset(new LazyInitSharedResource<DescriptorSetLayoutGenerator, BuildingModel>([this](std::unique_ptr<DescriptorSetLayoutGenerator>& descriptorSetLayoutGenerator)
		{
			descriptorSetLayoutGenerator.reset(new DescriptorSetLayoutGenerator);
			descriptorSetLayoutGenerator->addUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0); // mesh infos
		}));

	m_buildingDescriptorSetLayout.reset(new LazyInitSharedResource<DescriptorSetLayout, BuildingModel>([this](std::unique_ptr<DescriptorSetLayout>& descriptorSetLayout)
		{
			descriptorSetLayout.reset(new DescriptorSetLayout(m_buildingDescriptorSetLayoutGenerator->getResource()->getDescriptorLayouts()));
		}));

	m_defaultPipelineSet.reset(new LazyInitSharedResource<PipelineSet, BuildingModel>([this](std::unique_ptr<PipelineSet>& pipelineSet)
		{
			pipelineSet.reset(new PipelineSet);

			PipelineSet::PipelineInfo pipelineInfo;

			pipelineInfo.shaderInfos.resize(2);
			pipelineInfo.shaderInfos[0].shaderFilename = "Shaders/buildings/shader.vert";
			pipelineInfo.shaderInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			pipelineInfo.shaderInfos[1].shaderFilename = "Shaders/defaultPipeline/shader.frag";
			pipelineInfo.shaderInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

			// IA
			Vertex3D::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);
			InstanceData::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 1, static_cast<uint32_t>(pipelineInfo.vertexInputAttributeDescriptions.size()));

			pipelineInfo.vertexInputBindingDescriptions.resize(2);
			Vertex3D::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);
			InstanceData::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[1], 1);

			// Resources
			pipelineInfo.descriptorSetLayouts = { { m_modelDescriptorSetLayout->getResource()->getDescriptorSetLayout(), 1 },
				{ CommonDescriptorLayouts::g_commonDescriptorSetLayout, 2 }, { m_buildingDescriptorSetLayout->getResource()->getDescriptorSetLayout(), 3 } };
			pipelineInfo.bindlessDescriptorSlot = 0;
			pipelineInfo.cameraDescriptorSlot = 4;

			// Color Blend
			pipelineInfo.blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

			// Dynamic states
			pipelineInfo.dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

			// Rasterization
			pipelineInfo.cullMode = VK_CULL_MODE_NONE;

			const uint32_t pipelineIdx = pipelineSet->addPipeline(pipelineInfo);
			if (pipelineIdx != 0)
				Debug::sendError("Unexpected pipeline idx");
		}));

	m_window.getMeshWithMaterials().loadDefaultMesh(glm::vec3(1.0f, 0.0f, 0.4f));
	m_wall.getMeshWithMaterials().loadDefaultMesh(glm::vec3(0.5f, 0.25f, 0.0f));

	// Model descriptor set
	m_descriptorSet.reset(new DescriptorSet(m_modelDescriptorSetLayout->getResource()->getDescriptorSetLayout(), UpdateRate::NEVER));

	DescriptorSetGenerator descriptorSetGenerator(m_modelDescriptorSetLayoutGenerator->getResource()->getDescriptorLayouts());
	descriptorSetGenerator.setBuffer(0, *m_matricesUniformBuffer);
	m_descriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());

	// Building descriptor set
	auto initBufferAndDescriptorSet = [&](BuildingPiece& piece)
	{
		piece.getInfoUniformBuffer().reset(new Buffer(sizeof(MeshInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, UpdateRate::NEVER));
		piece.getDescriptorSet().reset(new DescriptorSet(m_buildingDescriptorSetLayout->getResource()->getDescriptorSetLayout(), UpdateRate::NEVER));
		DescriptorSetGenerator buildingDescriptorSetGenerator(m_buildingDescriptorSetLayoutGenerator->getResource()->getDescriptorLayouts());
		buildingDescriptorSetGenerator.setBuffer(0, *piece.getInfoUniformBuffer());
		piece.getDescriptorSet()->update(buildingDescriptorSetGenerator.getDescriptorSetCreateInfo());
	};

	initBufferAndDescriptorSet(m_window);
	initBufferAndDescriptorSet(m_wall);
	m_window.subscribe(this, [this] { m_needRebuild = true; });
	m_wall.subscribe(this, [this] { m_needRebuild = true; });

	rebuildRenderingInfos();
}

void BuildingModel::loadParams(Wolf::JSONReader& jsonReader)
{
	::loadParams(jsonReader, ID, m_buildingParams);
	::loadParams(jsonReader, ID, m_modelParams);
}

void BuildingModel::updateBeforeFrame()
{
	EditorModelInterface::updateBeforeFrame();

	if (m_needRebuild)
	{
		rebuildRenderingInfos();
	}

	m_window.getMeshWithMaterials().updateBeforeFrame();
	m_wall.getMeshWithMaterials().updateBeforeFrame();
}

void BuildingModel::getMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& outList)
{
	auto addPiece = [&](BuildingPiece& piece)
		{
			RenderMeshList::MeshToRenderInfo meshToRenderInfo(piece.getMeshWithMaterials().getMesh(), m_defaultPipelineSet->getResource());
			meshToRenderInfo.descriptorSets.push_back({ m_descriptorSet.createConstNonOwnerResource(), 1 });
			meshToRenderInfo.descriptorSets.push_back({ piece.getDescriptorSet().createConstNonOwnerResource(), 3 });
			meshToRenderInfo.instanceInfos = { piece.getInstanceBuffer().get(), piece.getInstanceCount() };

			outList.push_back(meshToRenderInfo);
		};

	addPiece(m_window);
	addPiece(m_wall);
}

void BuildingModel::activateParams()
{
	EditorModelInterface::activateParams();

	for (EditorParamInterface* param : m_buildingParams)
	{
		param->activate();
	}
}

void BuildingModel::addParamsToJSON(std::string& outJSON, uint32_t tabCount)
{
	EditorModelInterface::addParamsToJSON(outJSON, tabCount);
	::addParamsToJSON(outJSON, m_buildingParams, false, tabCount);
}

void BuildingModel::MeshWithMaterials::updateBeforeFrame()
{
	if (m_meshLoadingRequested)
	{
		loadMesh();
		m_meshLoadingRequested = false;
	}
}

void BuildingModel::MeshWithMaterials::loadMesh()
{
	Timer timer(static_cast<std::string>(m_loadingPathParam) + " loading");

	ModelLoadingInfo modelLoadingInfo;
	modelLoadingInfo.filename = static_cast<std::string>(m_loadingPathParam);
	std::string materialFolder = static_cast<std::string>(m_loadingPathParam).substr(0, static_cast<std::string>(m_loadingPathParam).find_last_of("\\"));
	modelLoadingInfo.mtlFolder = materialFolder;
	modelLoadingInfo.vulkanQueueLock = nullptr;
	modelLoadingInfo.materialLayout = MaterialLoader::InputMaterialLayout::EACH_TEXTURE_A_FILE;
	modelLoadingInfo.materialIdOffset = m_materialsGPUManager->getCurrentMaterialCount();
	ModelData windowMeshData;
	ModelLoader::loadObject(windowMeshData, modelLoadingInfo);

	m_mesh = std::move(windowMeshData.mesh);
	m_images.resize(windowMeshData.images.size());
	for (uint32_t i = 0; i < windowMeshData.images.size(); ++i)
	{
		m_images[i] = std::move(windowMeshData.images[i]);
	}
	const glm::vec3 meshSize = m_mesh->getAABB().getSize();
	m_sizeInMeter = glm::vec2(meshSize.x, meshSize.y);

	const glm::vec3 meshCenter = m_mesh->getAABB().getCenter();
	m_center = glm::vec2(meshCenter.x, meshCenter.y);

	addMaterialsToGPU();
	notifySubscribers();
}

void BuildingModel::MeshWithMaterials::loadDefaultMesh(const glm::vec3& color)
{
	const uint32_t materialIdOffset = m_materialsGPUManager->getCurrentMaterialCount();

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
	m_mesh.reset(new Mesh(vertices, indices));
	m_sizeInMeter = glm::vec2(2.0f, 2.0f);
	m_center = glm::vec2(0.0f, 0.0f);

	m_images.resize(5);
	constexpr uint32_t DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE = 32;
	auto createImage = [](ResourceUniqueOwner<Image>& image)
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
		ResourceUniqueOwner<Image>& image = m_images[0];
		createImage(image);

		std::array<Pixel, DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE * DEFAULT_IMAGE_PIXEL_COUNT_PER_SIDE> pixels{};
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
		ResourceUniqueOwner<Image>& image = m_images[1];
		createImage(image);
	}

	// Roughness
	{
		ResourceUniqueOwner<Image>& image = m_images[2];
		createImage(image);
	}

	// Metalness
	{
		ResourceUniqueOwner<Image>& image = m_images[3];
		createImage(image);
	}

	// AO
	{
		ResourceUniqueOwner<Image>& image = m_images[4];
		createImage(image);
	}

	addMaterialsToGPU();
}

void BuildingModel::MeshWithMaterials::addMaterialsToGPU() const
{
	std::vector<DescriptorSetGenerator::ImageDescription> imageDescriptions(m_images.size());
	for (uint32_t i = 0; i < m_images.size(); ++i)
	{
		imageDescriptions[i].imageView = m_images[i]->getDefaultImageView();
		imageDescriptions[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	m_materialsGPUManager->addNewMaterials(imageDescriptions);
}

void BuildingModel::MeshWithMaterials::requestMeshLoading()
{
	if (!std::string(m_loadingPathParam).empty())
		m_meshLoadingRequested = true;
}

float BuildingModel::computeFloorSize() const
{
	return m_fullSizeY / static_cast<float>(m_floorCountParam);
}

float BuildingModel::computeWindowCountOnSide(const glm::vec3& sideDir) const
{
	return std::ceil(length(glm::vec3(m_sizeXZParam.getValue().x, m_fullSizeY, m_sizeXZParam.getValue().y) * sideDir) / (m_window.getSideSizeInMeter() + m_wall.getSideSizeInMeter()));
}

void BuildingModel::rebuildRenderingInfos()
{
	std::vector<InstanceData> windowInstances;
	std::vector<InstanceData> wallInstances;

	for (uint32_t floorIdx = 0; floorIdx < m_floorCountParam; ++floorIdx)
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
				glm::vec3(m_sizeXZParam.getValue().x / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_sizeXZParam.getValue().y / 2.0f + m_window.getSideSizeInMeter() / 2.0f),
				glm::vec3(0.0f, 0.0f, 1.0f)
			},
			SideInfo {
				glm::vec3(-1.0f, 0.0f,  0.0f),
				glm::vec3(-m_sizeXZParam.getValue().x / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_sizeXZParam.getValue().y / 2.0f + m_window.getSideSizeInMeter() / 2.0f),
				glm::vec3(0.0f, 0.0f, 1.0f)
			},
			SideInfo {
				glm::vec3(0.0f, 0.0f,  1.0f),
				glm::vec3(-m_sizeXZParam.getValue().x / 2.0f + m_window.getSideSizeInMeter() / 2.0f, computeFloorSize() / 2.0f + floorHeight, m_sizeXZParam.getValue().y / 2.0f),
				glm::vec3(1.0f, 0.0f, 0.0f)
			},
			SideInfo {
				glm::vec3(0.0f, 0.0f, -1.0f),
				glm::vec3(-m_sizeXZParam.getValue().x / 2.0f + m_window.getSideSizeInMeter() / 2.0f, computeFloorSize() / 2.0f + floorHeight, -m_sizeXZParam.getValue().y / 2.0f),
				glm::vec3(1.0f, 0.0f, 0.0f)
			}
		};
		for (const SideInfo& sideInfo : sideInfos)
		{
			for (uint32_t windowIdx = 0, end = static_cast<uint32_t>(computeWindowCountOnSide(normalize(sideInfo.offsetScaleBetweenWindowCenters))); windowIdx < end; ++windowIdx)
			{
				const float spaceBetweenWindows = m_window.getSideSizeInMeter() + m_wall.getSideSizeInMeter();

				const glm::vec3 windowCenterPos = sideInfo.firstWindowPos + static_cast<float>(windowIdx) * (sideInfo.offsetScaleBetweenWindowCenters * spaceBetweenWindows);
				windowInstances.push_back({ windowCenterPos, sideInfo.normal });

				if (length(glm::vec3(m_sizeXZParam.getValue().x, m_fullSizeY, m_sizeXZParam.getValue().y) * sideInfo.offsetScaleBetweenWindowCenters) / (m_window.getSideSizeInMeter() + m_wall.getSideSizeInMeter()) != computeWindowCountOnSide(
					normalize(sideInfo.offsetScaleBetweenWindowCenters)) && windowIdx == end - 1)
					continue;

				const glm::vec3 wallCenterPos = sideInfo.firstWindowPos + static_cast<float>(windowIdx) * (sideInfo.offsetScaleBetweenWindowCenters * spaceBetweenWindows) + (sideInfo.offsetScaleBetweenWindowCenters * static_cast<float>(m_window.getSideSizeInMeter()));
				wallInstances.push_back({ wallCenterPos, sideInfo.normal });
			}
		}
	}

	auto updateBuffers = [](BuildingPiece& piece, const std::vector<InstanceData>& data)
	{
		piece.getInstanceBuffer().reset(new Buffer(data.size() * sizeof(InstanceData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, UpdateRate::NEVER));
		piece.getInstanceBuffer()->transferCPUMemoryWithStagingBuffer(data.data(), sizeof(InstanceData) * data.size());
		piece.setInstanceCount(static_cast<uint32_t>(data.size()));

		MeshInfo windowsInfo;
		windowsInfo.scale = glm::vec2(piece.getSideSizeInMeter() / piece.getMeshWithMaterials().getSizeInMeter().x, piece.getHeightInMeter() / piece.getMeshWithMaterials().getSizeInMeter().y);
		windowsInfo.offset = -piece.getMeshWithMaterials().getCenter();
		piece.getInfoUniformBuffer()->transferCPUMemory(&windowsInfo, sizeof(windowsInfo), 0);
	};
	updateBuffers(m_window, windowInstances);
	updateBuffers(m_wall, wallInstances);

	m_needRebuild = false;
}
