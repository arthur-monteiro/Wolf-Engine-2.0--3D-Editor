#pragma once

#include <chrono>

#include "ComponentInterface.h"
#include "EditorTypes.h"
#include "EntityContainer.h"
#include "GasCylinderComponent.h"
#include "ParticleEmitter.h"

class AnimatedModel;

class PlayerComponent : public ComponentInterface
{
public:
	static inline std::string ID = "playerComponent";
	std::string getId() const override { return ID; }

	PlayerComponent(std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> getEntityFromLoadingPathCallback, const Wolf::ResourceNonOwner<EntityContainer>& entityContainer, const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline);

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override;

	void saveCustom() const override {}

protected:
	void onEntityRegistered() override;

private:
	void updateAnimatedModel();

	inline static const std::string TAB = "Player component";
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;
	Wolf::ResourceNonOwner<EntityContainer> m_entityContainer;
	Wolf::ResourceNonOwner<UpdateGPUBuffersPass> m_updateGPUBuffersPass;

	EditorParamFloat m_speed = EditorParamFloat("Speed", TAB, "Movement", 1.0f, 10.0f);

	EditorParamFloat m_shootLength = EditorParamFloat("Shoot length", TAB, "Shoot", 1.0f, 30.0f);
	EditorParamFloat m_shootAngle = EditorParamFloat("Shoot angle in degree", TAB, "Shoot", 1.0f, 30.0f);
	EditorParamVector3 m_gunPositionOffset = EditorParamVector3("Gun position offset", TAB, "Shoot", 0.0f, 2.0f);
	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_contaminationEmitterEntity;
	void onContaminationEmitterChanged();
	EditorParamString m_contaminationEmitterParam = EditorParamString("Contamination Emitter", TAB, "Shoot", [this]() { onContaminationEmitterChanged(); }, EditorParamString::ParamStringType::ENTITY);
	Wolf::NullableResourceNonOwner<ParticleEmitter> m_smokeEmitterComponent;
	void onSmokeEmitterChanged();
	EditorParamString m_smokeEmitterParam = EditorParamString("Smoke Emitter", TAB, "Shoot", [this]() { onSmokeEmitterChanged(); }, EditorParamString::ParamStringType::ENTITY);

	Wolf::NullableResourceNonOwner<GasCylinderComponent> m_gasCylinderComponent;
	void onGasCylinderChanged();
	bool m_needCheckForNewLinkedEntities = false;
	EditorParamString m_gasCylinderParam = EditorParamString("Gas cylinder", TAB, "Gas cylinder", [this]() { onGasCylinderChanged(); }, EditorParamString::ParamStringType::ENTITY);
	EditorParamEnum m_gasCylinderTopBone = EditorParamEnum({}, "Top bone", TAB, "Gas cylinder");
	EditorParamVector3 m_gasCylinderTopBoneOffset = EditorParamVector3("Top bone offset", TAB, "Gas cylinder", -1.0f, 1.0f);
	EditorParamEnum m_gasCylinderBottomBone = EditorParamEnum({}, "Bottom bone", TAB, "Gas cylinder");
	EditorParamVector3 m_gasCylinderBottomBoneOffset = EditorParamVector3("Bottom bone offset", TAB, "Gas cylinder", -1.0f, 1.0f);

	EditorParamUInt m_gamepadIdx = EditorParamUInt("Gamepad Idx", TAB, "General", 0, Wolf::InputHandler::MAX_GAMEPAD_COUNT);

	std::unique_ptr<Wolf::ResourceNonOwner<AnimatedModel>> m_animatedModel;
	EditorParamEnum m_animationIdle = EditorParamEnum({ "Default" }, "Idle animation", TAB, "Animations");
	EditorParamEnum m_animationWalk = EditorParamEnum({ "Default" }, "Walk animation", TAB, "Animations");
	EditorParamEnum m_animationRun = EditorParamEnum({ "Default" }, "Run animation", TAB, "Animations");

	std::array<EditorParamInterface*, 15> m_editorParams =
	{
		&m_speed,
		&m_gamepadIdx,
		&m_shootLength,
		&m_shootAngle,
		&m_gunPositionOffset,
		&m_contaminationEmitterParam,
		&m_smokeEmitterParam,
		&m_animationIdle,
		&m_animationWalk,
		&m_animationRun,
		&m_gasCylinderParam,
		&m_gasCylinderTopBone,
		&m_gasCylinderTopBoneOffset,
		&m_gasCylinderBottomBone,
		&m_gasCylinderBottomBoneOffset
	};

	std::chrono::steady_clock::time_point m_lastUpdateTimePoint;

	// Shoot
	float m_currentShootX = 0.0f, m_currentShootY = 0.0f;
	glm::vec3 getGunPosition() const;
	bool canShoot() const;
	bool isShooting() const;

	// Gas cylinder management
	bool m_isWaitingForGasCylinderButtonRelease = false;
	uint32_t getGasCylinderCleanFlags() const;

	// Smoke
	float getMaxParticleSize() const;

	// Debug
	void buildShootDebugMesh();

	bool m_shootDebugMeshRebuildRequested = false;
	Wolf::ResourceUniqueOwner<Wolf::Mesh> m_shootDebugMesh;
	uint64_t m_lastShootRequestSentToContaminationEmitterTimer = 0;
};

