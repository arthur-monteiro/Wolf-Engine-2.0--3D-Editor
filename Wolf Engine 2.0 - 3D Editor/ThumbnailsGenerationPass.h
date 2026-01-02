#pragma once

#include <deque>

#include <CommandRecordBase.h>
#include <ShaderParser.h>

#include <GifEncoder.h>

#include "FirstPersonCamera.h"
#include "ModelLoader.h"

class ThumbnailsGenerationPass : public Wolf::CommandRecordBase
{
public:
	static constexpr uint32_t OUTPUT_SIZE = 100;
	static constexpr float DELAY_BETWEEN_ICON_FRAMES_MS = 40.0f;

	ThumbnailsGenerationPass();

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void addCameraForThisFrame(Wolf::CameraList& cameraList) const;

	class Request
	{
	public:
		Request(ModelData* modelData, uint32_t firstMaterialIdx, std::string outputFullFilepath, const std::function<void()>& onGeneratedCallback, const glm::mat4& viewMatrix);

	private:
		friend ThumbnailsGenerationPass;

		ModelData* m_modelData = nullptr;
		uint32_t m_firstMaterialIdx = 0;
		std::string m_outputFullFilepath;
		uint32_t m_imageLeftToDraw = 0;
		std::function<void()> m_onGeneratedCallback;
		glm::mat4 m_viewMatrix;
	};
	void addRequestBeforeFrame(const Request& request);

private:
	static uint32_t computeTotalImageToDraw(const Request& request);

	static constexpr Wolf::Format OUTPUT_FORMAT = Wolf::Format::R8G8B8A8_UNORM;

	std::deque<Request> m_pendingRequests;
	std::deque<Request> m_currentRequests;
	bool m_drawRecordedThisFrame = false;

	std::unique_ptr<Wolf::RenderPass> m_renderPass;
	std::unique_ptr<Wolf::FrameBuffer> m_frameBuffer;
	std::unique_ptr<Wolf::Image> m_renderTargetImage;
	std::unique_ptr<Wolf::Image> m_copyImage;
	std::unique_ptr<Wolf::Image> m_depthImage;



	std::unique_ptr<Wolf::ShaderParser> m_fragmentShaderParser;
	std::unique_ptr<Wolf::ShaderParser> m_staticVertexShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_staticPipeline;
	std::unique_ptr<Wolf::ShaderParser> m_animatedVertexShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_animatedPipeline;

	Wolf::DescriptorSetLayoutGenerator m_descriptorSetGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
	struct UniformBufferData
	{
		uint32_t firstMaterialIdx;
	};
	Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;

	Wolf::DescriptorSetLayoutGenerator m_animationDescriptorSetGenerator;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_animationDescriptorSetLayout;
	Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_animationDescriptorSet;
	Wolf::ResourceUniqueOwner<Wolf::Buffer> m_bonesBuffer;

	std::unique_ptr<Wolf::FirstPersonCamera> m_camera;

	GifEncoder m_gifEncoder;
};

