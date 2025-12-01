#pragma once

#include <CommandRecordBase.h>

#include "RayTracedWorldManager.h"

class UpdateRayTracedWorldPass : public Wolf::CommandRecordBase
{
public:
    UpdateRayTracedWorldPass(const Wolf::ResourceNonOwner<RayTracedWorldManager>& rayTracedWorldManager);

    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    bool wasEnabledThisFrame() const { return m_wasEnabledThisFrame; }

private:
    Wolf::ResourceNonOwner<RayTracedWorldManager> m_rayTracedWorldManager;

    bool m_wasEnabledThisFrame = false;
};
