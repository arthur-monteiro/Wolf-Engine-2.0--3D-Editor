#pragma once

#include <glm/glm.hpp>

#include <CommandRecordBase.h>
#include <ResourceUniqueOwner.h>

#include <Image.h>

#include "DynamicResourceUniqueOwnerArray.h"
#include "DynamicStableArray.h"

class UpdateGPUBuffersPass : public Wolf::CommandRecordBase
{
public:
	UpdateGPUBuffersPass() = default;

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
			if (m_outputOffset + m_size > m_outputBuffer->getSize())
			{
				Wolf::Debug::sendCriticalError("Wrong request, this could create a GPU hang");
			}
		}

		Request(const unsigned char* pixels, const Wolf::ResourceNonOwner<Wolf::Image>& outputImage, const Wolf::Image::TransitionLayoutInfo& finalLayout, uint32_t mipLevel,
			const glm::ivec2& copySize, const glm::ivec2& imageOffset) :
			m_data(pixels), m_outputImage(outputImage), m_finalLayout(finalLayout), m_mipLevel(mipLevel), m_copySize(copySize), m_imageOffset(imageOffset)
		{
			Wolf::Extent3D imageExtent = m_outputImage->getExtent();
			float imageDepth = static_cast<float>(imageExtent.depth);
			if (imageExtent.depth != 1)
			{
				imageDepth = static_cast<float>(imageExtent.depth >> mipLevel);
			}
			m_size = static_cast<uint32_t>(static_cast<float>(copySize.x) * static_cast<float>(copySize.y) * m_outputImage->getBPP());
		}

		Request(uint32_t fillData, uint32_t size, const Wolf::ResourceNonOwner<Wolf::Buffer>& outputBuffer, uint32_t outputOffset)
			: m_fillData(fillData), m_size(size), m_outputBuffer(outputBuffer), m_outputOffset(outputOffset)
		{
		}

		[[nodiscard]] uint32_t getSize() const { return m_size; }
		[[nodiscard]] const void* getData() const { return m_data; }
		[[nodiscard]] uint32_t getFillData() const { return m_fillData; }

		enum class ResourceType { BUFFER, IMAGE };
		[[nodiscard]] ResourceType getResourceType() const { return m_outputBuffer ? ResourceType::BUFFER : ResourceType::IMAGE; }
		[[nodiscard]] Wolf::ResourceNonOwner<Wolf::Buffer> getOutputBuffer() const { return m_outputBuffer; }
		[[nodiscard]] Wolf::ResourceNonOwner<Wolf::Image> getOutputImage() const { return m_outputImage; }

		[[nodiscard]] uint32_t getOutputOffset() const { return m_outputOffset; }
		[[nodiscard]] const Wolf::Image::TransitionLayoutInfo& getImageFinalLayout() const { return m_finalLayout; }
		[[nodiscard]] uint32_t getMipLevel() const { return m_mipLevel; }
		[[nodiscard]] const glm::ivec2& getCopySize() const { return m_copySize; }
		[[nodiscard]] const glm::ivec2& getImageOffset() const { return m_imageOffset; }

	private:
		const void* m_data = nullptr;
		uint32_t m_fillData = 0; // repeated data to fill the buffer
		uint32_t m_size = 0;
		Wolf::NullableResourceNonOwner<Wolf::Buffer> m_outputBuffer;
		Wolf::NullableResourceNonOwner<Wolf::Image> m_outputImage;
		uint32_t m_outputOffset = 0;
		Wolf::Image::TransitionLayoutInfo m_finalLayout{};
		uint32_t m_mipLevel = 0;
		glm::ivec2 m_copySize{};
		glm::ivec2 m_imageOffset{};
	};
	void addRequestBeforeFrame(const Request& request);

private:
	class InternalRequest
	{
	public:
		InternalRequest() = default;
		explicit InternalRequest(const Request& request) : m_request(request)
		{
			if (const void* data = m_request.getData())
			{
				m_stagingBuffer.reset(Wolf::Buffer::createBuffer(m_request.getSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
				m_stagingBuffer->transferCPUMemory(data, m_request.getSize());

				m_mode = Mode::COPY;
			}
			else
			{
				m_mode = Mode::FILL;

				if (m_request.getResourceType() != Request::ResourceType::BUFFER)
				{
					Wolf::Debug::sendCriticalError("Fill request must be for a buffer");
				}
			}
		}
		InternalRequest(const InternalRequest& other) = delete;

		void recordCommands(const Wolf::CommandBuffer* commandBuffer) const;

	private:
		enum class Mode { COPY, FILL };
		Mode m_mode;

		void recordCopyToBuffer(const Wolf::CommandBuffer* commandBuffer) const;
		void recordFillBuffer(const Wolf::CommandBuffer* commandBuffer) const;
		void recordCopyToImage(const Wolf::CommandBuffer* commandBuffer) const;

		Request m_request;
		Wolf::ResourceUniqueOwner<Wolf::Buffer> m_stagingBuffer;
	};

	Wolf::DynamicResourceUniqueOwnerArray<InternalRequest, 4> m_addRequestsQueue;
	std::mutex m_addRequestQueueMutex;
	Wolf::DynamicStableArray<Wolf::DynamicResourceUniqueOwnerArray<InternalRequest, 4>, 2> m_currentRequestsQueues;

	bool m_transferRecordedThisFrame = false;
};

