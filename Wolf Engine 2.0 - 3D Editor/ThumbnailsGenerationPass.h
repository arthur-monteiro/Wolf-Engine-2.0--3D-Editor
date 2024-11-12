#pragma once

#include <deque>

#include <CommandRecordBase.h>
#include <ShaderParser.h>

#include "FirstPersonCamera.h"
#include "ModelLoader.h"

class ThumbnailsGenerationPass : public Wolf::CommandRecordBase
{
public:
	ThumbnailsGenerationPass();

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void addCameraForThisFrame(Wolf::CameraList& cameraList) const;

	struct Request
	{
		Wolf::ModelData* modelData = nullptr;
		std::string outputFullFilepath;
	};
	void addRequestBeforeFrame(const Request& request);

private:
	static constexpr uint32_t OUTPUT_SIZE = 100;
	static constexpr VkFormat OUTPUT_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

	std::deque<Request> m_pendingRequests;
	std::deque<Request> m_currentRequests;
	bool m_drawRecordedThisFrame = false;

	std::unique_ptr<Wolf::RenderPass> m_renderPass;
	std::unique_ptr<Wolf::FrameBuffer> m_frameBuffer;
	std::unique_ptr<Wolf::Image> m_renderTargetImage;
	std::unique_ptr<Wolf::Image> m_copyImage;
	std::unique_ptr<Wolf::Image> m_depthImage;

	std::unique_ptr<Wolf::ShaderParser> m_vertexShaderParser;
	std::unique_ptr<Wolf::ShaderParser> m_fragmentShaderParser;
	std::unique_ptr<Wolf::Pipeline> m_pipeline;

	std::unique_ptr<Wolf::FirstPersonCamera> m_camera;
};

