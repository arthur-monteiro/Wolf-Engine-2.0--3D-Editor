#pragma once

#include <CommandRecordBase.h>
#include <ResourceNonOwner.h>

#include <Buffer.h>
#include <DynamicResourceUniqueOwnerArray.h>
#include <ReadableBuffer.h>

#include "DrawIdsPass.h"
#include "ForwardPass.h"

class GPUBufferToGPUBufferCopyPass: public Wolf::CommandRecordBase
{
public:
    GPUBufferToGPUBufferCopyPass(const Wolf::ResourceNonOwner<const ForwardPass>& forwardPass, const Wolf::ResourceNonOwner<const DrawIdsPass>& drawIdsPass);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    [[nodiscard]] bool isCurrentQueueEmpty() const;

    class Request
    {
    public:
        Request() = default;
        Request(const Wolf::ResourceNonOwner<Wolf::Buffer>& srcBuffer, uint32_t srcOffset, const Wolf::ResourceNonOwner<Wolf::ReadableBuffer>& readableBuffer, uint32_t size)
            : m_srcBuffer(srcBuffer), m_srcOffset(srcOffset), m_readableBuffer(readableBuffer), m_size(size)
        {
        }

        void recordCommands(const Wolf::CommandBuffer* commandBuffer) const;

    private:
        Wolf::NullableResourceNonOwner<Wolf::Buffer> m_srcBuffer;
        uint32_t m_srcOffset = 0;
        Wolf::NullableResourceNonOwner<Wolf::ReadableBuffer> m_readableBuffer;
        uint32_t m_size = 00;
    };
    void addRequestBeforeFrame(const Request& request);

private:
    Wolf::ResourceNonOwner<const ForwardPass> m_forwardPass;
    Wolf::ResourceNonOwner<const DrawIdsPass> m_drawIdsPass;

    static constexpr uint32_t RequestsBatchSize = 4;
    static constexpr uint32_t QueuesBatchSize = 2;

    Wolf::DynamicResourceUniqueOwnerArray<Request, RequestsBatchSize> m_addRequestsQueue;
    std::mutex m_addRequestQueueMutex;
    Wolf::DynamicStableArray<Wolf::DynamicResourceUniqueOwnerArray<Request, RequestsBatchSize>, QueuesBatchSize> m_currentRequestsQueues;

    bool m_commandsRecordedThisFrame = false;
};
