#pragma once

#include <glm/glm.hpp>

#include <CommandRecordBase.h>
#include <DynamicResourceUniqueOwnerArray.h>
#include <FrameBuffer.h>
#include <RenderPass.h>

#include "CameraInterface.h"
#include "Image.h"

class CustomDepthPass : public Wolf::CommandRecordBase
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
        Request() = default;
        Request(const Wolf::ResourceNonOwner<Wolf::Image>& outputDepth, const Wolf::ResourceNonOwner<Wolf::CameraInterface>& camera) : m_outputDepth(outputDepth), m_camera(camera)
        {
            initResources();
        }
        Request(Request& other) : m_outputDepth(other.m_outputDepth), m_camera(other.m_camera)
        {
            m_renderPass.reset(other.m_renderPass.release());
            m_frameBuffer.reset(other.m_frameBuffer.release());
        }

        void recordCommands(const Wolf::CommandBuffer* commandBuffer, const Wolf::RecordContext& context);

        uint32_t getCameraIdx() const { return m_cameraIdx; }
        void setCameraIdx(uint32_t cameraIdx) { m_cameraIdx = cameraIdx; }

        Wolf::ResourceNonOwner<Wolf::CameraInterface>& getCamera() { return m_camera; }

    private:
        Wolf::ResourceNonOwner<Wolf::Image> m_outputDepth;
        Wolf::ResourceNonOwner<Wolf::CameraInterface> m_camera;

        void initResources();
        Wolf::ResourceUniqueOwner<Wolf::RenderPass> m_renderPass;
        Wolf::ResourceUniqueOwner<Wolf::FrameBuffer> m_frameBuffer;
        uint32_t m_cameraIdx = -1;
    };
    void addRequestBeforeFrame(Request& request);

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
