#pragma once

#include <GPUDataTransfersManager.h>

class GPUBufferToGPUBufferCopyPass;
class UpdateGPUBuffersPass;

class EditorGPUDataTransfersManager : public Wolf::GPUDataTransfersManagerInterface
{
public:
    EditorGPUDataTransfersManager() = default;
    ~EditorGPUDataTransfersManager() override = default;

    void setUpdateGPUBuffersPass(const Wolf::ResourceNonOwner<UpdateGPUBuffersPass>& updateGPUBuffersPass);
    void setGPUBufferToGPUBufferCopyPass(const Wolf::ResourceNonOwner<GPUBufferToGPUBufferCopyPass>& gpuBufferToGPUBufferCopyPass);

    void pushDataToGPUBuffer(const void* data, uint32_t size, const Wolf::ResourceNonOwner<Wolf::Buffer>& outputBuffer, uint32_t outputOffset) override;
    void fillGPUBuffer(uint32_t fillValue, uint32_t size, const Wolf::ResourceNonOwner<Wolf::Buffer>& outputBuffer, uint32_t outputOffset) override;
    void pushDataToGPUImage(const PushDataToGPUImageInfo& pushDataToGPUImageInfo) override;

    void requestGPUBufferReadbackRecord(const Wolf::ResourceNonOwner<Wolf::Buffer>& srcBuffer, uint32_t srcOffset, const Wolf::ResourceNonOwner<Wolf::ReadableBuffer>& readableBuffer, uint32_t size) override;

private:
    Wolf::NullableResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBufferPass;
    Wolf::NullableResourceNonOwner<GPUBufferToGPUBufferCopyPass> m_gpuBufferToGPUBufferCopyPass;
};
