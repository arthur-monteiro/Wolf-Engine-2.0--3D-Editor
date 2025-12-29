#pragma once

#include <glm/glm.hpp>

#include <CameraInterface.h>
#include <CommandRecordBase.h>
#include <DescriptorSetLayoutGenerator.h>
#include <DynamicResourceUniqueOwnerArray.h>
#include <FrameBuffer.h>
#include <Image.h>
#include <RenderPass.h>
#include <UniformBuffer.h>

#include "GameContext.h"

class CustomSceneRenderPass : public Wolf::CommandRecordBase
{
public:
    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    void updateBeforeFrame(Wolf::CameraList& cameraList);

    class Request
    {
    public:
        struct Output
        {
            Wolf::ResourceNonOwner<Wolf::Image> m_image;
            GameContext::DisplayType m_displayType;

            Output(const Wolf::ResourceNonOwner<Wolf::Image>& image, GameContext::DisplayType displayType) : m_image(image), m_displayType(displayType) {}
        };

        Request() = default;
        Request(const Wolf::ResourceNonOwner<Wolf::Image>& outputDepth, const Wolf::ResourceNonOwner<Wolf::CameraInterface>& camera, const std::vector<Output>& outputs)
            : m_outputDepth(outputDepth), m_camera(camera), m_outputs(outputs)
        {
            initResources();
        }
        Request(Request& other) : m_outputDepth(other.m_outputDepth), m_camera(other.m_camera), m_outputs(other.m_outputs)
        {
            m_renderPass.reset(other.m_renderPass.release());
            m_frameBuffer.reset(other.m_frameBuffer.release());

            m_descriptorSetLayoutGenerator = other.m_descriptorSetLayoutGenerator;
            m_descriptorSet.reset(other.m_descriptorSet.release());
            m_descriptorSetLayout.reset(other.m_descriptorSetLayout.release());
        }

        void recordCommands(const Wolf::CommandBuffer* commandBuffer, const Wolf::RecordContext& context);

        void pause() { m_isRunning = false; }
        void unpause() { m_isRunning = true; }
        bool isRunning() const { return m_isRunning; }

        uint32_t getCameraIdx() const { return m_cameraIdx; }
        void setCameraIdx(uint32_t cameraIdx) { m_cameraIdx = cameraIdx; }

        Wolf::ResourceNonOwner<Wolf::CameraInterface>& getCamera() { return m_camera; }

    private:
        Wolf::ResourceNonOwner<Wolf::Image> m_outputDepth;
        std::vector<Output> m_outputs;
        Wolf::ResourceNonOwner<Wolf::CameraInterface> m_camera;

        void initResources();
        Wolf::ResourceUniqueOwner<Wolf::RenderPass> m_renderPass;
        Wolf::ResourceUniqueOwner<Wolf::FrameBuffer> m_frameBuffer;
        uint32_t m_cameraIdx = -1;

        struct UniformBufferData
        {
            glm::vec4 padding;
        };
        Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_uniformBuffer;
        Wolf::DescriptorSetLayoutGenerator m_descriptorSetLayoutGenerator;
        Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_descriptorSet;
        Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_descriptorSetLayout;

        bool m_isRunning = true;
    };
    using RequestId = uint32_t;
    static constexpr RequestId NO_REQUEST_ID = -1;

    [[nodiscard]] RequestId addRequestBeforeFrame(Request& request);
    void updateRequestBeforeFrame(RequestId requestIdx, Request& request);

    bool isRequestRunning(RequestId requestIdx) const;
    void pauseRequestBeforeFrame(RequestId requestIdx);
    void unpauseRequestBeforeFrame(RequestId requestIdx);

    bool commandsRecordedThisFrame() const { return m_commandsRecordedThisFrame; }

private:
    void addCamerasForThisFrame(Wolf::CameraList& cameraList) const;

    static constexpr uint32_t RequestsBatchSize = 4;
    static constexpr uint32_t QueuesBatchSize = 2;

    Wolf::DynamicResourceUniqueOwnerArray<Request, RequestsBatchSize> m_addRequestsQueue;
    std::mutex m_addRequestQueueMutex;
    Wolf::DynamicResourceUniqueOwnerArray<Request, RequestsBatchSize> m_currentRequestsQueue;

    bool m_commandsRecordedThisFrame = false;
};
