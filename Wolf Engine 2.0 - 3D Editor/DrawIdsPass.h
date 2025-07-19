#pragma once

#include <CommandRecordBase.h>

#include "EditorParams.h"
#include "PreDepthPass.h"
#include "ShadowMaskPassInterface.h"

class DrawIdsPass : public Wolf::CommandRecordBase
{
public:
    DrawIdsPass(EditorParams* editorParams, const Wolf::ResourceNonOwner<ShadowMaskPassInterface>& shadowMaskPass, const Wolf::ResourceNonOwner<PreDepthPass>& preDepthPass)
        : m_editorParams(editorParams), m_shadowMaskPass(shadowMaskPass), m_preDepthPass(preDepthPass) {}

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

private:
    static constexpr Wolf::Format OUTPUT_FORMAT = Wolf::Format::R32_UINT;

    static Wolf::Attachment setupColorAttachment(const Wolf::InitializationContext& context);
    Wolf::Attachment setupDepthAttachment(const Wolf::InitializationContext& context);

    EditorParams* m_editorParams;
    Wolf::ResourceNonOwner<ShadowMaskPassInterface> m_shadowMaskPass;
    Wolf::ResourceNonOwner<PreDepthPass> m_preDepthPass;

    Wolf::ResourceUniqueOwner<Wolf::Image> m_outputImage;
};
