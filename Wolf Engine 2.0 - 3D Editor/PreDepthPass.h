#pragma once

#include <CommandRecordBase.h>
#include <DepthPassBase.h>
#include <DescriptorSet.h>
#include <Image.h>

#include "EditorParams.h"
#include "UpdateGPUBuffersPass.h"

class SceneElements;

class PreDepthPass : public Wolf::CommandRecordBase, public Wolf::DepthPassBase
{
public:
	PreDepthPass(EditorParams* editorParams, bool copyOutput, const Wolf::ResourceNonOwner<UpdateGPUBuffersPass>& updateGPUBuffersPass)
		: m_editorParams(editorParams), m_copyOutput(copyOutput), m_updateGPUBuffersPass(updateGPUBuffersPass) {}

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	Wolf::Image* getOutput() const override { return m_depthImage.get(); }
	Wolf::Image* getCopy() const { return m_copyImage.get(); }

private:
	void createCopyImage(Wolf::Format format);

	uint32_t getWidth() override { return m_swapChainWidth; }
	uint32_t getHeight() override { return m_swapChainHeight; }
	Wolf::ImageUsageFlags getAdditionalUsages() override { return Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::TRANSFER_SRC; }
	VkImageLayout getFinalLayout() override { return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; }

	void recordDraws(const Wolf::RecordContext& context) override;
	const Wolf::CommandBuffer& getCommandBuffer(const Wolf::RecordContext& context) override;

	EditorParams* m_editorParams;

	/* Pipeline */
	uint32_t m_swapChainWidth = 0;
	uint32_t m_swapChainHeight = 0;

	/* Resources */
	std::unique_ptr<Wolf::Image> m_copyImage;

	/* Params */
	bool m_copyOutput;

	/* Extern */
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
};

