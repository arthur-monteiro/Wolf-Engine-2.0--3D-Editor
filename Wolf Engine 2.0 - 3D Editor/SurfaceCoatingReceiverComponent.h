#pragma once

#include "ComponentInterface.h"

class SurfaceCoatingReceiverComponent : public ComponentInterface
{
public:
    static inline std::string ID = "surfaceCoatingReceiver";
    std::string getId() const override { return ID; }

    void loadParams(Wolf::JSONReader& jsonReader) override;
    void activateParams() override;
    void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

    void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
    void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override;
    void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

    void saveCustom() const override {}
};
