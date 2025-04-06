#pragma once

#include <CommandRecordBase.h>
#include <ResourceUniqueOwner.h>

#include <Image.h>

#include "DynamicResourceUniqueOwnerArray.h"
#include "DynamicStableArray.h"

class UpdateGPUBuffersPass : public Wolf::CommandRecordBase
{
public:
	UpdateGPUBuffersPass();

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	void clear();

	bool transferRecordedThisFrame() const { return m_transferRecordedThisFrame; }

	class Request
	{
	public:
		Request() = default;
		Request(const void* data, uint32_t size, const Wolf::ResourceNonOwner<Wolf::Buffer>& outputBuffer, uint32_t outputOffset) :
			m_data(data), m_size(size), m_outputBuffer(outputBuffer), m_outputOffset(outputOffset)
		{		
		}

		Request(const unsigned char* pixels, const Wolf::ResourceNonOwner<Wolf::Image>& outputImage, const Wolf::Image::TransitionLayoutInfo& finalLayout, uint32_t mipLevel) :
			m_data(pixels), m_outputImage(outputImage), m_finalLayout(finalLayout), m_mipLevel(mipLevel)
		{
			Wolf::Extent3D imageExtent = m_outputImage->getExtent();
			float imageDepth = static_cast<float>(imageExtent.depth);
			if (imageExtent.depth != 1)
			{
				imageDepth = static_cast<float>(imageExtent.depth >> mipLevel);
			}
			m_size = static_cast<uint32_t>(static_cast<float>(imageExtent.width >> mipLevel) * static_cast<float>(imageExtent.height >> mipLevel) * imageDepth * m_outputImage->getBPP());
		}

		uint32_t getSize() const { return m_size; }
		const void* getData() const { return m_data; }

		enum class ResourceType { BUFFER, IMAGE };
		ResourceType getResourceType() const { return m_outputBuffer ? ResourceType::BUFFER : ResourceType::IMAGE; }
		Wolf::ResourceNonOwner<Wolf::Buffer> getOutputBuffer() const { return m_outputBuffer; }
		Wolf::ResourceNonOwner<Wolf::Image> getOutputImage() const { return m_outputImage; }

		uint32_t getOutputOffset() const { return m_outputOffset; }
		const Wolf::Image::TransitionLayoutInfo& getImageFinalLayout() const { return m_finalLayout; }
		uint32_t getMipLevel() const { return m_mipLevel; }

	private:
		const void* m_data = nullptr;
		uint32_t m_size = 0;
		Wolf::NullableResourceNonOwner<Wolf::Buffer> m_outputBuffer;
		Wolf::NullableResourceNonOwner<Wolf::Image> m_outputImage;
		uint32_t m_outputOffset = 0;
		Wolf::Image::TransitionLayoutInfo m_finalLayout{};
		uint32_t m_mipLevel = 0;
	};
	void addRequestBeforeFrame(const Request& request);

private:
	class InternalRequest
	{
	public:
		InternalRequest() = default;
		InternalRequest(const Request& request) : m_request(request)
		{
			m_stagingBuffer.reset(Wolf::Buffer::createBuffer(m_request.getSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
			m_stagingBuffer->transferCPUMemory(m_request.getData(), m_request.getSize());
		}
		InternalRequest(const InternalRequest& other) = delete;

		void recordCommands(const Wolf::CommandBuffer* commandBuffer) const;

	private:
		void recordCopyToBuffer(const Wolf::CommandBuffer* commandBuffer) const;
		void recordCopyToImage(const Wolf::CommandBuffer* commandBuffer) const;

	private:
		Request m_request;

		Wolf::ResourceUniqueOwner<Wolf::Buffer> m_stagingBuffer;
	};

	Wolf::DynamicResourceUniqueOwnerArray<InternalRequest, 4> m_addRequestsQueue;
	std::mutex m_addRequestQueueMutex;
	Wolf::DynamicStableArray<Wolf::DynamicResourceUniqueOwnerArray<InternalRequest, 4>, 2> m_currentRequestsQueues;

	bool m_transferRecordedThisFrame = false;
};

