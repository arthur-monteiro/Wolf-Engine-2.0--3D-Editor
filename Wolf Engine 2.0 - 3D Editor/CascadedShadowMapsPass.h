#pragma once

#include <array>
#include <glm/glm.hpp>

#include <CameraList.h>
#include <DepthPassBase.h>
#include <OrthographicCamera.h>

class CascadeDepthPass : public Wolf::DepthPassBase
{
public:
	CascadeDepthPass(const Wolf::InitializationContext& context, uint32_t width, uint32_t height, const Wolf::CommandBuffer* commandBuffer, uint32_t cameraIdx);
	CascadeDepthPass(const CascadeDepthPass&) = delete;

	void getViewProjMatrix(glm::mat4& output) const { output = m_camera->getProjectionMatrix() * m_camera->getViewMatrix(); }
	void setCameraInfos(const glm::vec3& center, float radius, const glm::vec3& direction) const;

	void addCameraForThisFrame(Wolf::CameraList& cameraList) const { cameraList.addCameraForThisFrame(m_camera.get(), m_cameraIdx); }

private:
	uint32_t getWidth() override { return m_width; }
	uint32_t getHeight() override { return m_height; }

	void recordDraws(const Wolf::RecordContext& context) override;
	const Wolf::CommandBuffer& getCommandBuffer(const Wolf::RecordContext& context) override;
	Wolf::ImageUsageFlags getAdditionalUsages() override { return Wolf::ImageUsageFlagBits::SAMPLED; }
	Wolf::ImageLayout getFinalLayout() override { return Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL; }

	/* Shared resources */
	const Wolf::CommandBuffer* m_commandBuffer;

	/* Owned resources */
	std::unique_ptr<Wolf::OrthographicCamera> m_camera;
	uint32_t m_cameraIdx;
	uint32_t m_width, m_height;
};

class CascadedShadowMapsPass : public Wolf::CommandRecordBase
{
public:
	static constexpr int CASCADE_COUNT = 4;

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void addCamerasForThisFrame(Wolf::CameraList& cameraList) const;
	void setFar(float far);

	Wolf::Image* getShadowMap(uint32_t cascadeIdx) const { return m_cascadeDepthPasses[cascadeIdx]->getOutput(); }
	float getCascadeSplit(uint32_t cascadeIdx) const { return m_cascadeSplits[cascadeIdx]; }
	void getCascadeMatrix(uint32_t cascadeIdx, glm::mat4& output) const { m_cascadeDepthPasses[cascadeIdx]->getViewProjMatrix(output); }
	uint32_t getCascadeTextureSize(uint32_t cascadeIdx) const { return m_cascadeTextureSize[cascadeIdx]; }
	bool wasEnabledThisFrame() const { return m_wasEnabledThisFrame; }
	float getFar() const { return m_far; }

private:
	void computeCascadeSplits();

	/* Cascades */
	uint32_t m_cascadeTextureSize[CASCADE_COUNT] = { 3072, 3072, 3072, 3072 };
	std::array<std::unique_ptr<CascadeDepthPass>, CASCADE_COUNT> m_cascadeDepthPasses;
	float m_near = 0.1f;
	float m_far = 50.0f;
	std::array<float, CASCADE_COUNT> m_cascadeSplits{};

	bool m_wasEnabledThisFrame = false;
};