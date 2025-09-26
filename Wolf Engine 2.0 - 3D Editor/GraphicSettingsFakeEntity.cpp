#include "GraphicSettingsFakeEntity.h"

#include "ComputeSkyCubeMapPass.h"
#include "EditorParamsHelper.h"

GraphicSettingsFakeEntity::GraphicSettingsFakeEntity(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline)
: Entity("", [](Entity*){}), m_renderingPipeline(renderingPipeline)
{
}

void GraphicSettingsFakeEntity::activateParams() const
{
    for (EditorParamInterface* param : m_editorParams)
    {
        param->activate();
    }
}

void GraphicSettingsFakeEntity::fillJSONForParams(std::string& outJSON)
{
    outJSON += "{\n";
    outJSON += "\t" R"("params": [)" "\n";
    addParamsToJSON(outJSON, m_editorParams, true);
    if (const size_t commaPos = outJSON.substr(outJSON.size() - 3).find(','); commaPos != std::string::npos)
    {
        outJSON.erase(commaPos + outJSON.size() - 3);
    }
    outJSON += "\t]\n";
    outJSON += "}";
}

void GraphicSettingsFakeEntity::updateBeforeFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler, const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<DrawManager>& drawManager,
    const Wolf::ResourceNonOwner<EditorPhysicsManager>& editorPhysicsManager)
{
    if (m_skyCubeMapResolutionChanged)
    {
        m_renderingPipeline->getSkyBoxManager()->setCubeMapResolution(m_skyCubeMapResolution);
        m_skyCubeMapResolutionChanged = false;
    }
}

void GraphicSettingsFakeEntity::save() const
{
    // Nothing to do for now
}