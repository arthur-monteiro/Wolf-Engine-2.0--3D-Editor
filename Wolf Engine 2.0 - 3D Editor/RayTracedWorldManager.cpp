#include "RayTracedWorldManager.h"

void RayTracedWorldManager::buildTLAS(const TLASInfo& info)
{
    std::vector<Wolf::BLASInstance> blasInstances(info.m_instances.size());
    for (size_t i = 0; i < blasInstances.size(); i++)
    {
        blasInstances[i].bottomLevelAS = info.m_instances[i].m_bottomLevelAccelerationStructure.operator->();
        blasInstances[i].transform = info.m_instances[i].m_transform;
        blasInstances[i].instanceID = i;
        blasInstances[i].hitGroupIndex = 0;
    }

    m_topLevelAccelerationStructure.reset(Wolf::TopLevelAccelerationStructure::createTopLevelAccelerationStructure(blasInstances));
}
