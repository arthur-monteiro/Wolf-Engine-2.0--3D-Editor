#pragma once

#include "ComponentInterface.h"
#include "EditorTypes.h"
#include "MaterialEditor.h"
#include "RenderingPipelineInterface.h"

class ParticleEmitter : public ComponentInterface
{
public:
	static inline std::string ID = "particleEmitter";
	std::string getId() const override { return ID; }

	ParticleEmitter(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback, const std::function<void(ComponentInterface*)>& requestReloadCallback);
	~ParticleEmitter() override;

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer) override;
	void alterMeshesToRender(std::vector<Wolf::RenderMeshList::MeshToRenderInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void updateDuringFrame(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override {}
	bool requiresInputs() const override { return false; }

	// Spawn
	glm::vec3 getSpawnPosition() const;
	uint32_t getSpawnShape() const;
	float getSpawnShapeRadiusRadiusOrWidth() const;
	float getSpawnShapeHeight() const;
	float getSpawnBoxDepth() const;

	uint32_t getMaxParticleCount() const { return m_maxParticleCount; }
	glm::vec3 getDirection() const { return m_normalizedDirection; }
	float getSpeed() const { return m_speed; }
	uint32_t getLifetimeInMs() const { return static_cast<uint32_t>(m_lifetime * 1000.0f); }
	uint32_t getMaterialIdx() const { return m_materialIdx; }
	void computeOpacity(std::vector<float>& outValues, uint32_t valueCount) const { m_particleOpacity.computeValues(outValues, valueCount); }
	void computeSize(std::vector<float>& outValues, uint32_t valueCount) const { m_particleSize.computeValues(outValues, valueCount); }

	uint64_t getNextSpawnTimerInMs() const { return m_nextSpawnMsTimer; }
	uint32_t getNextParticleToSpawnIdx() const { return m_nextParticleToSpawnIdx; }
	uint32_t getNextParticleToSpawnCount() const { return m_nextParticleToSpawnCount; }

private:
	void forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString);

	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	inline static const std::string TAB = "Particle emitter";

	// ----- Spawn -----
	static constexpr uint32_t CYLINDER_SHAPE = 0;
	static constexpr uint32_t BOX_SHAPE = 1;
	EditorParamEnum m_spawnShape = EditorParamEnum({ "Cylinder", "Box" }, "Shape", TAB, "Spawn", [this]() { m_requestReloadCallback(this); });

	// Cylinder shape
	EditorParamVector3 m_spawnCylinderCenterPosition = EditorParamVector3("Cylinder center position", TAB, "Spawn", -10.0f, 10.0f);
	EditorParamFloat m_spawnCylinderHeight = EditorParamFloat("Cylinder height", TAB, "Spawn", 0.0f, 10.0f);
	EditorParamFloat m_spawnCylinderRadius = EditorParamFloat("Cylinder radius", TAB, "Spawn", 0.0f, 10.0f);
	std::array<EditorParamInterface*, 3> m_spawnShapeCylinderParams =
	{
		&m_spawnCylinderCenterPosition,
		&m_spawnCylinderHeight,
		&m_spawnCylinderRadius
	};

	// Box shape
	EditorParamVector3 m_spawnBoxCenterPosition = EditorParamVector3("Box center position", TAB, "Spawn", -10.0f, 10.0f);
	EditorParamFloat m_spawnBoxWidth = EditorParamFloat("Box width", TAB, "Spawn", 0.0f, 10.0f);
	EditorParamFloat m_spawnBoxHeight = EditorParamFloat("Box height", TAB, "Spawn", 0.0f, 10.0f);
	EditorParamFloat m_spawnBoxDepth = EditorParamFloat("Box depth", TAB, "Spawn", 0.0f, 10.0f);
	std::array<EditorParamInterface*, 4> m_spawnShapeBoxParams =
	{
		&m_spawnBoxCenterPosition,
		&m_spawnBoxWidth,
		&m_spawnBoxHeight,
		&m_spawnBoxDepth
	};

	EditorParamUInt m_maxParticleCount = EditorParamUInt("Max particle count", TAB, "Spawn", 1, 1024);
	EditorParamFloat m_delayBetweenTwoParticles = EditorParamFloat("Delay between 2 particles (sec)", TAB, "Spawn", 0.0f, 10.0f);

	// ----- Movement -----
	void updateNormalizedDirection();
	EditorParamVector3 m_direction = EditorParamVector3("Direction", TAB, "Movement", -1.0f, 1.0f, [this]() { updateNormalizedDirection(); });
	EditorParamFloat m_speed = EditorParamFloat("Speed (m/s)", TAB, "Movement", 1.0f, 10.0f);

	// ----- Death -----
	EditorParamFloat m_lifetime = EditorParamFloat("Lifetime (sec)", TAB, "Death", 0.1f, 10.0f);

	// ----- Particle -----
	void onParticleSizeChanged();
	EditorParamCurve m_particleSize = EditorParamCurve("Size", TAB, "Particle", [this]() { onParticleSizeChanged(); });
	void onParticleOpacityChanged();
	EditorParamCurve m_particleOpacity = EditorParamCurve("Opacity", TAB, "Particle", [this]() { onParticleOpacityChanged(); });
	std::unique_ptr<Wolf::ResourceNonOwner<Entity>> m_particleEntity;
	void onParticleEntityChanged();
	void onParticleDataChanged();
	EditorParamString m_particleEntityParam = EditorParamString("Particle entity", TAB, "Particle", [this]() { onParticleEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);

	std::array<EditorParamInterface*, 15> m_allEditorParams =
	{
		&m_spawnShape,

		&m_spawnCylinderCenterPosition,
		&m_spawnCylinderHeight,
		&m_spawnCylinderRadius,

		&m_spawnBoxCenterPosition,
		&m_spawnBoxWidth,
		&m_spawnBoxHeight,

		&m_maxParticleCount,
		&m_delayBetweenTwoParticles,

		&m_direction,
		&m_speed,

		&m_lifetime,

		&m_particleSize,
		&m_particleOpacity,
		&m_particleEntityParam
	};

	std::array<EditorParamInterface*, 9> m_alwaysVisibleEditorParams =
	{
		&m_spawnShape,

		&m_maxParticleCount,
		&m_delayBetweenTwoParticles,

		&m_direction,
		&m_speed,

		&m_lifetime,

		&m_particleSize,
		&m_particleOpacity,
		&m_particleEntityParam
	};

	Wolf::ResourceNonOwner<ParticleUpdatePass> m_particleUpdatePass;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	// Info for current execution
	uint64_t m_nextSpawnMsTimer = 0;
	uint64_t m_lastSpawnMsTimer = 0;

	uint32_t m_nextParticleToSpawnIdx = 0;
	uint32_t m_nextParticleToSpawnCount = 0;

	glm::vec3 m_normalizedDirection = glm::vec3(0.0f, 1.0f, 0.0f);
	uint32_t m_materialIdx = 0;

	bool m_particleNotificationRegistered = false;
};
