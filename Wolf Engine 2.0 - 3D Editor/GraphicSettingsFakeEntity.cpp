#include "GraphicSettingsFakeEntity.h"

#include "ComputeSkyCubeMapPass.h"
#include "EditorParamsHelper.h"
#include "SystemManager.h"

GraphicSettingsFakeEntity::GraphicSettingsFakeEntity(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, SystemManager* systemManager, const std::function<void(ComponentInterface*)>& requestReloadCallback)
: Entity("", [](Entity*){}, [](Entity*){}), m_renderingPipeline(renderingPipeline), m_systemManager(systemManager), m_requestReloadCallback(requestReloadCallback)
{
    m_skyCubeMapResolution = m_renderingPipeline->getSkyBoxManager()->getCubeMapResolution();
    m_csmFar = m_renderingPipeline->getCascadedShadowMapsPass()->getFar();
    m_enableTrilinearVoxelGI = true;

    if (m_renderingPipeline->hasRayTracing())
    {
        m_globalIrradianceTechnique.setOptions({"Voxel GI"});
    }
    else
    {
        m_globalIrradianceTechnique.setOptions({"Ambient lighting"});
    }
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
        bool isLastShadow = m_shadowTechnique == 0 && e == static_cast<EditorParamInterface*>(&m_csmFar) ||
           m_shadowTechnique != 0 && e == static_cast<EditorParamInterface*>(&m_shadowTechnique);
        bool isShadowLast = reinterpret_cast<const std::string&>(m_voxelGIParams) == "Ambient lighting";
        bool isLastGI = e == static_cast<EditorParamInterface*>(&m_probePositionVoxelGIDebug);
        bool isLast = (isLastShadow && isShadowLast) || isLastGI;

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

    const std::string& selectedGlobalIrradiance = static_cast<const std::string&>(m_globalIrradianceTechnique);
    if (selectedGlobalIrradiance == "Voxel GI")
    {
        for (EditorParamInterface* editorParam : m_voxelGIParams)
        {
            callback(editorParam, inOutString);
        }
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

void GraphicSettingsFakeEntity::updateGlobalIrradianceTechnique()
{
    // Nothing to do
}

void GraphicSettingsFakeEntity::onTrilinarVoxelGIChanged()
{
    m_renderingPipeline->getForwardPass()->setEnableTrilinearVoxelGI(m_enableTrilinearVoxelGI);
}

void GraphicSettingsFakeEntity::onVoxelGIDebugChanged()
{
    m_renderingPipeline->getVoxelGIPass()->setEnableDebug(m_enableVoxelGIDebug);
}

void GraphicSettingsFakeEntity::onProbePositionVoxelGIDebugChanged()
{
    m_renderingPipeline->getVoxelGIPass()->setDebugPostionFace(m_probePositionVoxelGIDebug);
}
