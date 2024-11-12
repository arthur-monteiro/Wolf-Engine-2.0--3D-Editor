#pragma once

#include <CommandRecordBase.h>
#include <ResourceUniqueOwner.h>

class UpdateGPUBuffersPass : public Wolf::CommandRecordBase
{
public:
	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	bool transferRecordedThisFrame() const { return m_transferRecordedThisFrame; }

	class Request
	{
	public:
		Request(const void* data, uint32_t size, const Wolf::ResourceNonOwner<Wolf::Buffer>& outputBuffer, uint32_t outputOffset) :
			m_data(data), m_size(size), m_outputBuffer(outputBuffer), m_outputOffset(outputOffset)
		{		
		}

		uint32_t getSize() const { return m_size; }
		const void* getData() const { return m_data; }
		const Wolf::ResourceNonOwner<Wolf::Buffer>& getOutputBuffer() const { return m_outputBuffer; }
		uint32_t getOutputOffset() const { return m_outputOffset; }

	private:
		const void* m_data;
		uint32_t m_size;
		Wolf::ResourceNonOwner<Wolf::Buffer> m_outputBuffer;
		uint32_t m_outputOffset;
	};
	void addRequestBeforeFrame(const Request& request);

private:
	class InternalRequest
	{
	public:
		InternalRequest(Request request) : m_request(std::move(request))
		{
			m_stagingBuffer.reset(Wolf::Buffer::createBuffer(m_request.getSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
			m_stagingBuffer->transferCPUMemory(m_request.getData(), m_request.getSize());
		}

		void recordCopyToBuffer(Wolf::CommandBuffer* commandBuffer) const;

	private:
		Request m_request;

		Wolf::ResourceUniqueOwner<Wolf::Buffer> m_stagingBuffer;
	};

	std::vector<InternalRequest> m_addRequestsQueue;
	std::vector<InternalRequest> m_currentRequestsQueue;

	bool m_transferRecordedThisFrame = false;
};

