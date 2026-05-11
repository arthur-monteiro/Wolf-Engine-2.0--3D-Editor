#pragma once

#include "AssetId.h"
#include "ComponentInterface.h"
#include "EditorTypes.h"

class AssetManager;

class CombinedImageEditor : public ComponentInterface
{
public:
    static inline std::string ID = "combinedImageEditorEditor";
    std::string getId() const override { return ID; }

    CombinedImageEditor(AssetManager* assetManager);
    CombinedImageEditor(const CombinedImageEditor&) = delete;

    void loadParams(Wolf::JSONReader& jsonReader) override;
    void activateParams() override;
    void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

    void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}

    void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
    void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

    void saveCustom() const override {}

    std::string getLoadingPathR() const { return m_filePathRParam; }
    std::string getLoadingPathG() const { return m_filePathGParam; }
    std::string getLoadingPathB() const { return m_filePathBParam; }
    std::string getLoadingPathA() const { return m_filePathAParam; }

    AssetId getAssetId(uint32_t channelIdx) const { return m_imageAssetIds[channelIdx]; }
    void setAssetId(uint32_t channelIdx, AssetId assetId) { m_imageAssetIds[channelIdx] = assetId; }

private:
    inline static const std::string TAB = "Image";
    AssetManager* m_assetManager;

    void onFilePathChanged(EditorParamString& param, uint32_t channelIdx);
    std::array<AssetId, 4> m_imageAssetIds = { NO_ASSET, NO_ASSET, NO_ASSET, NO_ASSET };

    EditorParamString m_filePathRParam = EditorParamString("File channel R", TAB, "File", [this]() { onFilePathChanged(m_filePathRParam, 0); }, EditorParamString::ParamStringType::ASSET);
    EditorParamString m_filePathGParam = EditorParamString("File channel G", TAB, "File", [this]() { onFilePathChanged(m_filePathGParam, 1); }, EditorParamString::ParamStringType::ASSET);
    EditorParamString m_filePathBParam = EditorParamString("File channel B", TAB, "File", [this]() { onFilePathChanged(m_filePathBParam, 2); }, EditorParamString::ParamStringType::ASSET);
    EditorParamString m_filePathAParam = EditorParamString("File channel A", TAB, "File", [this]() { onFilePathChanged(m_filePathAParam, 3); }, EditorParamString::ParamStringType::ASSET);

    std::array<EditorParamInterface*, 4> m_params
    {
        &m_filePathRParam,
        &m_filePathGParam,
        &m_filePathBParam,
        &m_filePathAParam
    };
};
