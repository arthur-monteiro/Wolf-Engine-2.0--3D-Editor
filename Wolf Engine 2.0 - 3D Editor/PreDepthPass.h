#pragma once

#include <CommandRecordBase.h>
#include <DepthPassBase.h>
#include <DescriptorSet.h>
#include <Image.h>

#include "ComputeVertexDataPass.h"
#include "CustomSceneRenderPass.h"
#include "EditorParams.h"
#include "UpdateGPUBuffersPass.h"

class SceneElements;

class PreDepthPass : public Wolf::CommandRecordBase, public Wolf::DepthPassBase
{
public:
	PreDepthPass(EditorParams* editorParams, bool copyOutput, const Wolf::ResourceNonOwner<UpdateGPUBuffersPass>& updateGPUBuffersPass, const Wolf::ResourceNonOwner<ComputeVertexDataPass>& computeVertexDataPass,
		const Wolf::ResourceNonOwner<CustomSceneRenderPass>& customSceneRenderPass)
		: m_editorParams(editorParams), m_copyOutput(copyOutput), m_updateGPUBuffersPass(updateGPUBuffersPass), m_computeVertexDataPass(computeVertexDataPass), m_customSceneRenderPass(customSceneRenderPass) {}

	void initializeResources(const Wolf::InitializationContext& context) override;
	void resize(const Wolf::InitializationContext& context) override;
	void record(const Wolf::RecordContext& context) override;
	void submit(const Wolf::SubmitContext& context) override;

	Wolf::Image* getOutput() const override { return m_depthImage.get(); }

private:
	uint32_t getWidth() override { return m_swapChainWidth; }
	uint32_t getHeight() override { return m_swapChainHeight; }
	Wolf::ImageUsageFlags getAdditionalUsages() override { return Wolf::ImageUsageFlagBits::SAMPLED | Wolf::ImageUsageFlagBits::TRANSFER_SRC; }
	Wolf::ImageLayout getFinalLayout() override { return Wolf::ImageLayout::SHADER_READ_ONLY_OPTIMAL; }

	void recordDraws(const Wolf::RecordContext& context) override;
	const Wolf::CommandBuffer& getCommandBuffer(const Wolf::RecordContext& context) override;

	EditorParams* m_editorParams;

	/* Pipeline */
	uint32_t m_swapChainWidth = 0;
	uint32_t m_swapChainHeight = 0;

	/* Params */
	bool m_copyOutput;

	/* Extern */
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;
	Wolf::ResourceNonOwner<ComputeVertexDataPass> m_computeVertexDataPass;
	Wolf::ResourceNonOwner<CustomSceneRenderPass> m_customSceneRenderPass;
};

