#include "GraphicSettingsFakeEntity.h"

#include "ComputeSkyCubeMapPass.h"
#include "EditorParamsHelper.h"
#include "SystemManager.h"

GraphicSettingsFakeEntity::GraphicSettingsFakeEntity(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, SystemManager* systemManager, const std::function<void(ComponentInterface*)>& requestReloadCallback)
: Entity("", [](Entity*){}), m_renderingPipeline(renderingPipeline), m_systemManager(systemManager), m_requestReloadCallback(requestReloadCallback)
{
    m_skyCubeMapResolution = m_renderingPipeline->getSkyBoxManager()->getCubeMapResolution();
    m_csmFar = m_renderingPipeline->getCascadedShadowMapsPass()->getFar();
}

void GraphicSettingsFakeEntity::activateParams()
{
    std::string unused;
    forAllVisibleParams([](EditorParamInterface* e, std::string&) { e->activate(); }, unused);
}

void GraphicSettingsFakeEntity::fillJSONForParams(std::string& outJSON)
{
    outJSON += "{\n";
    outJSON += "\t" R"("params": [)" "\n";

    forAllVisibleParams([this](EditorParamInterface* e, std::string& inOutJSON) mutable
    {
        bool isLast = m_shadowTechnique == 0 && e == static_cast<EditorParamInterface*>(&m_csmFar) ||
           m_shadowTechnique != 0 && e == static_cast<EditorParamInterface*>(&m_shadowTechnique);
        e->addToJSON(inOutJSON, 1, isLast);
    }, outJSON);

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

void GraphicSettingsFakeEntity::forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString)
{
    for (EditorParamInterface* editorParam : m_alwaysVisibleParams)
    {
        callback(editorParam, inOutString);
    }

    switch (static_cast<uint32_t>(m_shadowTechnique))
    {
        case 0: // CSM
            {
                for (EditorParamInterface* editorParam : m_csmParams)
                {
                    callback(editorParam, inOutString);
                }
                break;
            }
        case 1: // Ray traced
            {
                break;
            }
        default:
            Wolf::Debug::sendError("Undefined shadow technique");
            break;
    }
}

GameContext::ShadowTechnique GraphicSettingsFakeEntity::computeShadowTechnique() const
{
    switch (static_cast<uint32_t>(m_shadowTechnique))
    {
        case 0:
            return GameContext::ShadowTechnique::CSM;
        case 1:
            return GameContext::ShadowTechnique::RAY_TRACED;
        default:
            Wolf::Debug::sendError("Unknown shadow technique");
            return GameContext::ShadowTechnique::CSM;
    }
}

void GraphicSettingsFakeEntity::updateShadowTechnique()
{
    GameContext& gameContext = m_systemManager->getInModificationGameContext();
    gameContext.shadowTechnique = computeShadowTechnique();

    m_requestReloadCallback(nullptr);
}

void GraphicSettingsFakeEntity::onCSMFarChanged()
{
    m_renderingPipeline->getCascadedShadowMapsPass()->setFar(m_csmFar);
}

void GraphicSettingsFakeEntity::onVoxelGIDebugChanged()
{
    m_renderingPipeline->getVoxelGIPass()->setEnableDebug(m_enableVoxelGIDebug);
}
