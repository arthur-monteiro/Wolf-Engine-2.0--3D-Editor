#pragma once

#include <CommandRecordBase.h>

#include "EditorParams.h"
#include "ForwardPass.h"
#include "PreDepthPass.h"

class DrawIdsPass : public Wolf::CommandRecordBase
{
public:
    DrawIdsPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass, const Wolf::ResourceNonOwner<const ForwardPass>& forwardPass)
        : m_editorParams(editorParams), m_preDepthPass(preDepthPass), m_forwardPass(forwardPass) {}

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    using PixelRequestCallback = std::function<void(uint32_t)>;
    void requestPixelBeforeFrame(uint32_t posX, uint32_t posY, const PixelRequestCallback& callback);
    bool isEnabledThisFrame() const { return m_lastRecordFrameNumber == Wolf::g_runtimeContext->getCurrentCPUFrameNumber() ? m_recordCommandsThisFrame : m_recordCommandsNextFrame; };
    void setIsFinalPassThisFrame() { m_finalPassFrameIdx = Wolf::g_runtimeContext->getCurrentCPUFrameNumber(); }

private:
    static constexpr Wolf::Format OUTPUT_FORMAT = Wolf::Format::R32_UINT;

    Wolf::Attachment setupColorAttachment(const Wolf::InitializationContext& context);
    Wolf::Attachment setupDepthAttachment(const Wolf::InitializationContext& context);
    void initializeFramesBuffer(const Wolf::InitializationContext& context, Wolf::Attachment& colorAttachment, Wolf::Attachment& depthAttachment);

    Wolf::ResourceUniqueOwner<Wolf::RenderPass> m_renderPass;
    Wolf::ResourceUniqueOwner<Wolf::FrameBuffer> m_frameBuffer;

    EditorParams* m_editorParams;
    Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;
    Wolf::ResourceNonOwner<const ForwardPass> m_forwardPass;

    uint32_t m_lastRecordFrameNumber = 0;
    uint32_t m_finalPassFrameIdx = static_cast<uint32_t>(-1);

    bool m_recordCommandsNextFrame = false;
    bool m_recordCommandsThisFrame = false;
    uint32_t m_requestPixelPosX = 0;
    uint32_t m_requestPixelPosY = 0;
    PixelRequestCallback m_requestPixelCallback;
    static constexpr uint32_t NO_REQUEST = -1;
    uint32_t m_requestFrameNumber = NO_REQUEST;

    Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;
    Wolf::ResourceUniqueOwner<Wolf::Image> m_copyImage;
};
