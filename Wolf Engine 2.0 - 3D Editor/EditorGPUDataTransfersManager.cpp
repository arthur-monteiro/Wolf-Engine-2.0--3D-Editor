#include "EditorGPUDataTransfersManager.h"

#include "GPUBufferToGPUBufferCopyPass.h"

void EditorGPUDataTransfersManager::clear()
{
    m_updateGPUBufferPass.release();
    m_gpuBufferToGPUBufferCopyPass.release();
}

void EditorGPUDataTransfersManager::setUpdateGPUBuffersPass(const Wolf::ResourceNonOwner<UpdateGPUBuffersPass>& UpdateGPUBuffersPass)
{
    m_updateGPUBufferPass = UpdateGPUBuffersPass;
}

void EditorGPUDataTransfersManager::setGPUBufferToGPUBufferCopyPass(const Wolf::ResourceNonOwner<GPUBufferToGPUBufferCopyPass>& gpuBufferToGPUBufferCopyPass)
{
    m_gpuBufferToGPUBufferCopyPass = gpuBufferToGPUBufferCopyPass;
}

void EditorGPUDataTransfersManager::pushDataToGPUBuffer(const void* data, uint32_t size,const Wolf::ResourceNonOwner<Wolf::Buffer>& outputBuffer, uint32_t outputOffset)
{
    if (!m_updateGPUBufferPass)
    {
        Wolf::Debug::sendCriticalError("Rendering pipeline hasn't been set");
        return;
    }

    m_updateGPUBufferPass->addRequestBeforeFrame({ data, size, outputBuffer, outputOffset });
}

void EditorGPUDataTransfersManager::fillGPUBuffer(uint32_t fillValue, uint32_t size, const Wolf::ResourceNonOwner<Wolf::Buffer>& outputBuffer, uint32_t outputOffset)
{
    if (!m_updateGPUBufferPass)
    {
        Wolf::Debug::sendCriticalError("Rendering pipeline hasn't been set");
        return;
    }

    m_updateGPUBufferPass->addRequestBeforeFrame({ fillValue, size, outputBuffer, outputOffset });
}

void EditorGPUDataTransfersManager::pushDataToGPUImage(const PushDataToGPUImageInfo& pushDataToGPUImageInfo)
{
    if (!m_updateGPUBufferPass)
    {
        Wolf::Debug::sendCriticalError("Rendering pipeline hasn't been set");
        return;
    }

    glm::ivec2 realCopySize = pushDataToGPUImageInfo.m_copySize;
    if (realCopySize.x == 0 && realCopySize.y == 0)
    {
        const Wolf::Extent3D imageExtent = { pushDataToGPUImageInfo.m_outputImage->getExtent().width >> pushDataToGPUImageInfo.m_mipLevel,
            pushDataToGPUImageInfo.m_outputImage->getExtent().height >> pushDataToGPUImageInfo.m_mipLevel, pushDataToGPUImageInfo.m_outputImage->getExtent().depth };
        realCopySize = glm::ivec2(imageExtent.width, imageExtent.height);
    }

    m_updateGPUBufferPass->addRequestBeforeFrame({ pushDataToGPUImageInfo.m_pixels, pushDataToGPUImageInfo.m_outputImage, pushDataToGPUImageInfo.m_finalLayout, pushDataToGPUImageInfo.m_mipLevel,
        realCopySize, pushDataToGPUImageInfo.m_imageOffset });
}

void EditorGPUDataTransfersManager::requestGPUBufferReadbackRecord(const Wolf::ResourceNonOwner<Wolf::Buffer>& srcBuffer, uint32_t srcOffset, const Wolf::ResourceNonOwner<Wolf::ReadableBuffer>& readableBuffer, uint32_t size)
{
    if (!m_gpuBufferToGPUBufferCopyPass)
    {
        Wolf::Debug::sendCriticalError("Rendering pipeline hasn't been set");
        return;
    }

    m_gpuBufferToGPUBufferCopyPass->addRequestBeforeFrame({ srcBuffer, srcOffset, readableBuffer, size });
}
