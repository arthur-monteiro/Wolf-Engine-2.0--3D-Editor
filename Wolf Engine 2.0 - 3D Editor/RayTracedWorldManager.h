#pragma once

#include <ResourceUniqueOwner.h>
#include <TopLevelAccelerationStructure.h>

class RayTracedWorldManager
{
public:
    struct TLASInfo
    {
        struct InstanceInfo
        {
            Wolf::ResourceNonOwner<Wolf::BottomLevelAccelerationStructure> m_bottomLevelAccelerationStructure;
            glm::mat4 m_transform;
        };
        std::vector<InstanceInfo> m_instances;
    };
    void buildTLAS(const TLASInfo& info);

    Wolf::ResourceNonOwner<Wolf::TopLevelAccelerationStructure> getTopLevelAccelerationStructure() { return m_topLevelAccelerationStructure.createNonOwnerResource(); }

private:
    Wolf::ResourceUniqueOwner<Wolf::TopLevelAccelerationStructure> m_topLevelAccelerationStructure;
};
