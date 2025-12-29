#pragma once

#include <OrthographicCamera.h>

#include <glm/gtc/constants.hpp>

#include "ComponentInterface.h"
#include "CustomSceneRenderPass.h"
#include "EditorTypes.h"
#include "Particle.h"
#include "TextureSetEditor.h"
#include "RenderingPipelineInterface.h"

class ParticleEmitter : public ComponentInterface
{
public:
	static inline std::string ID = "particleEmitter";
	std::string getId() const override { return ID; }

	ParticleEmitter(const Wolf::ResourceNonOwner<RenderingPipelineInterface>& renderingPipeline, const std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)>& getEntityFromLoadingPathCallback, 
		const std::function<void(ComponentInterface*)>& requestReloadCallback);
	~ParticleEmitter() override;

	void loadParams(Wolf::JSONReader& jsonReader) override;
	void activateParams() override;
	void addParamsToJSON(std::string& outJSON, uint32_t tabCount) override;

	void updateBeforeFrame(const Wolf::Timer& globalTimer, const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler) override;
	void alterMeshesToRender(std::vector<DrawManager::DrawMeshInfo>& renderMeshList) override {}
	void addDebugInfo(DebugRenderingManager& debugRenderingManager) override {}

	void saveCustom() const override {}

	// Spawn
	void requestEmission() { m_isEmitting = true; }
	void stopEmission() { m_isEmitting = false; }
	void setSpawnPosition(const glm::vec3& position);
	void setDirection(const glm::vec3& direction);

	// Particle
	void setColor(const glm::vec3& color) { m_particleColor = color; }

	uint32_t getMaxParticleCount() const { return m_maxParticleCount; }

	// Spawn
	glm::vec3 getSpawnPosition() const;
	uint32_t getSpawnShape() const;
	float getSpawnShapeRadiusRadiusOrWidth() const;
	float getSpawnShapeHeight() const;
	float getSpawnBoxDepth() const;

	// Movement
	glm::vec3 getDirection() const { return m_normalizedDirection; }
	float getMinSpeed() const { return m_minSpeed; }
	float getMaxSpeed() const { return m_maxSpeed; }
	uint32_t getDirectionShape() const { return m_directionShape; }
	float getDirectionConeAngle() const { return m_directionConeAngle; }

	// Death
	uint32_t getMinLifetimeInMs() const { return static_cast<uint32_t>(m_minLifetime * 1000.0f); }
	uint32_t getMaxLifetimeInMs() const { return static_cast<uint32_t>(m_maxLifetime * 1000.0f); }

	// Particle
	uint32_t getMaterialIdx() const { return m_materialIdx; }
	uint32_t getFlipBookSizeX() const { return m_flipBookSizeX; }
	uint32_t getFlipBookSizeY() const { return m_flipBookSizeY; }
	uint32_t getFirstFlipBookIdx() const { return m_firstFlipBookIdx; }
	uint32_t getFirstFlipBookRandomRange() const { return m_firstFlipBookRandomRange; }
	uint32_t getUsedTileCountInFlipBook() const { return m_usedTileCountInFlipBook; }
	void computeOpacity(std::vector<float>& outValues, uint32_t valueCount) const { m_particleOpacity.computeValues(outValues, valueCount); }
	void computeSize(std::vector<float>& outValues, uint32_t valueCount) const { m_particleSize.computeValues(outValues, valueCount); }
	float getMinSizeMultiplier() const { return m_particleMinSizeMultiplier; }
	float getMaxSizeMultiplier() const { return m_particleMaxSizeMultiplier; }
	float getOrientationMinAngle() const { return m_particleOrientationMinAngle; }
	float getOrientationMaxAngle() const { return m_particleOrientationMaxAngle; }
	glm::vec3 getParticleColor() const { return m_particleColor; }

	static constexpr uint32_t COLLISION_TYPE_NONE = 0;
	static constexpr uint32_t COLLISION_TYPE_DEPTH = 1;
	uint32_t getCollisionType() const { return m_collisionType; }
	void getCollisionViewProjMatrix(glm::mat4& outMatrix) const { outMatrix = m_depthCollisionCamera->getProjectionMatrix() * m_depthCollisionCamera->getViewMatrix(); }
	uint32_t getCollisionDepthTextureIdx() const { return m_collisionDepthTextureIdx; }
	float getCollisionDepthScale() const { return m_collisionDepthScale; }
	float getCollisionDepthOffset() const { return m_collisionDepthOffset; }
	static constexpr uint32_t COLLISION_BEHAVIOUR_DIE = 0;
	static constexpr uint32_t COLLISION_BEHAVIOUR_STOP = 1;
	uint32_t getCollisionBehaviour() const { return m_collisionBehaviour; }

	uint64_t getNextSpawnTimerInMs() const { return m_nextSpawnMsTimer; }
	uint32_t getNextParticleToSpawnIdx() const { return m_nextParticleToSpawnIdx; }
	uint32_t getNextParticleToSpawnCount() const { return m_nextParticleToSpawnCount; }
	float getDelayBetweenTwoParticlesInMs() const { return m_delayBetweenTwoParticles * 1000.0f; }

private:
	void onEntityRegistered() override;
	void forAllVisibleParams(const std::function<void(EditorParamInterface*, std::string& inOutString)>& callback, std::string& inOutString);

	std::function<void(ComponentInterface*)> m_requestReloadCallback;
	inline static const std::string TAB = "Particle emitter";

	// ----- Spawn -----
	EditorParamBool m_isEmitting = EditorParamBool("Emits", TAB, "Spawn");
	EditorParamUInt m_firstFlipBookIdx = EditorParamUInt("First flip-book index", TAB, "Spawn", 0, 1);
	EditorParamUInt m_firstFlipBookRandomRange = EditorParamUInt("First flip-book random range", TAB, "Spawn", 0, 1);
	void onUsedTileCountInFlipBookChanged();
	EditorParamUInt m_usedTileCountInFlipBook = EditorParamUInt("Number of tiles used in flip-book", TAB, "Spawn", 0, 1, [this]() { onUsedTileCountInFlipBookChanged(); });
	static constexpr uint32_t SPAWN_CYLINDER_SHAPE = 0;
	static constexpr uint32_t SPAWN_BOX_SHAPE = 1;
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

	void onParticleCountChanged();
	EditorParamUInt m_maxParticleCount = EditorParamUInt("Max particle count", TAB, "Spawn", 1, 1024, [this]() { onParticleCountChanged();});
	EditorParamFloat m_delayBetweenTwoParticles = EditorParamFloat("Delay between 2 particles (sec)", TAB, "Spawn", 0.0f, 10.0f);

	// ----- Movement -----
	EditorParamFloat m_minSpeed = EditorParamFloat("Speed min (m/s)", TAB, "Movement", 0.0f, 10.0f);
	EditorParamFloat m_maxSpeed = EditorParamFloat("Speed max (m/s)", TAB, "Movement", 0.0f, 10.0f);

	static constexpr uint32_t DIRECTION_CONE_SHAPE = 0;
	EditorParamEnum m_directionShape = EditorParamEnum({ "Cone"}, "Shape", TAB, "Movement", [this]() { m_requestReloadCallback(this); });

	void updateNormalizedDirection();
	// Cone shape
	EditorParamVector3 m_directionConeDirection = EditorParamVector3("Cone direction", TAB, "Movement", -1.0f, 1.0f, [this]() { updateNormalizedDirection(); });
	EditorParamFloat m_directionConeAngle = EditorParamFloat("Cone radius", TAB, "Movement", 0.0f, glm::pi<float>());
	std::array<EditorParamInterface*, 2> m_directionConeParams =
	{
		&m_directionConeDirection,
		&m_directionConeAngle
	};

	// ----- Death -----
	EditorParamFloat m_minLifetime = EditorParamFloat("Lifetime min (sec)", TAB, "Death", 0.1f, 10.0f);
	EditorParamFloat m_maxLifetime = EditorParamFloat("Lifetime max (sec)", TAB, "Death", 0.1f, 10.0f);

	// ----- Particle -----
	void onParticleSizeChanged();
	EditorParamCurve m_particleSize = EditorParamCurve("Size", TAB, "Particle", [this]() { onParticleSizeChanged(); });
	EditorParamFloat m_particleMinSizeMultiplier = EditorParamFloat("Size multiplier min", TAB, "Particle", 0.1f, 10.0f, [this]() { onParticleSizeChanged(); });
	EditorParamFloat m_particleMaxSizeMultiplier = EditorParamFloat("Size multiplier max", TAB, "Particle", 0.1f, 10.0f, [this]() { onParticleSizeChanged(); });
	void onParticleOpacityChanged();
	EditorParamCurve m_particleOpacity = EditorParamCurve("Opacity", TAB, "Particle", [this]() { onParticleOpacityChanged(); });
	EditorParamFloat m_particleOrientationMinAngle = EditorParamFloat("Orientation min (rad)", TAB, "Particle", 0.0f, glm::pi<float>());
	EditorParamFloat m_particleOrientationMaxAngle = EditorParamFloat("Orientation max (rad)", TAB, "Particle", 0.0f, glm::pi<float>());
	Wolf::NullableResourceNonOwner<Particle> m_particleComponent;
	void onParticleEntityChanged();
	void onParticleDataChanged();
	EditorParamString m_particleEntityParam = EditorParamString("Particle entity", TAB, "Particle", [this]() { onParticleEntityChanged(); }, EditorParamString::ParamStringType::ENTITY);
	EditorParamVector3 m_particleColor = EditorParamVector3("Color", TAB, "Particle", 0.0f, 1.0f);

	// ----- Collisions -----
	void onCollisionTypeChanged();
	EditorParamEnum m_collisionType = EditorParamEnum({ "None", "Custom depth"}, "Collision type", TAB, "Collisions", [this]() { onCollisionTypeChanged(); });

	// Custom depth
	EditorParamUInt m_collisionDepthTextureSize = EditorParamUInt("Depth texture size", TAB, "Collisions", 128, 4096);
	std::array<EditorParamInterface*, 1> m_collisionDepthParams =
	{
		&m_collisionDepthTextureSize
	};

	// Collision enabled
	void onCollisionBehaviourChanged();
	EditorParamEnum m_collisionBehaviour = EditorParamEnum({ "Die", "Stop" }, "Collision behaviour", TAB, "Collisions", [this]() { onCollisionBehaviourChanged(); });
	std::array<EditorParamInterface*, 1> m_collisionEnabledParams =
	{
		&m_collisionBehaviour
	};

	std::array<EditorParamInterface*, 32> m_allEditorParams =
	{
		&m_isEmitting,
		&m_firstFlipBookIdx,
		&m_firstFlipBookRandomRange,
		&m_usedTileCountInFlipBook,
		&m_spawnShape,

		&m_spawnCylinderCenterPosition,
		&m_spawnCylinderHeight,
		&m_spawnCylinderRadius,

		&m_spawnBoxCenterPosition,
		&m_spawnBoxWidth,
		&m_spawnBoxHeight,
		&m_spawnBoxDepth,

		&m_maxParticleCount,
		&m_delayBetweenTwoParticles,

		&m_minSpeed,
		&m_maxSpeed,
		&m_directionShape,
		&m_directionConeDirection,
		&m_directionConeAngle,

		&m_minLifetime,
		&m_maxLifetime,

		&m_particleSize,
		&m_particleMinSizeMultiplier,
		&m_particleMaxSizeMultiplier,
		&m_particleOrientationMinAngle,
		&m_particleOrientationMaxAngle,
		&m_particleOpacity,
		&m_particleEntityParam,
		&m_particleColor,

		&m_collisionType,
		&m_collisionDepthTextureSize,
		&m_collisionBehaviour
	};

	std::array<EditorParamInterface*, 21> m_alwaysVisibleEditorParams =
	{
		&m_isEmitting,
		&m_firstFlipBookIdx,
		&m_firstFlipBookRandomRange,
		&m_usedTileCountInFlipBook,
		&m_spawnShape,

		&m_maxParticleCount,
		&m_delayBetweenTwoParticles,

		&m_minSpeed,
		&m_maxSpeed,
		&m_directionShape,

		&m_minLifetime,
		&m_maxLifetime,

		&m_particleSize,
		&m_particleMinSizeMultiplier,
		&m_particleMaxSizeMultiplier,
		&m_particleOrientationMinAngle,
		&m_particleOrientationMaxAngle,
		&m_particleOpacity,
		&m_particleEntityParam,
		&m_particleColor,

		&m_collisionType
	};

	Wolf::ResourceNonOwner<ParticleUpdatePass> m_particleUpdatePass;
	Wolf::ResourceNonOwner<CustomSceneRenderPass> m_customDepthPass;
	std::function<Wolf::ResourceNonOwner<Entity>(const std::string&)> m_getEntityFromLoadingPathCallback;

	// Info for current execution
	uint64_t m_nextSpawnMsTimer = 0;
	uint64_t m_lastSpawnMsTimer = 0;

	uint32_t m_nextParticleToSpawnIdx = 0;
	uint32_t m_nextParticleToSpawnCount = 0;

	glm::vec3 m_normalizedDirection = glm::vec3(0.0f, 1.0f, 0.0f);
	uint32_t m_materialIdx = 0;
	uint32_t m_flipBookSizeX = 1;
	uint32_t m_flipBookSizeY = 1;

	bool m_particleNotificationRegistered = false;
	bool m_needCheckForNewLinkedEntities = true;

	// Graphic resources
	bool m_needToRegisterDepthTexture = false;
	uint32_t m_registeredDepthTextureIdx = CustomSceneRenderPass::NO_REQUEST_ID;

	void createDepthCollisionImage();
	Wolf::ResourceUniqueOwner<Wolf::Image> m_depthCollisionImage;
	uint32_t m_collisionDepthTextureIdx = -1;

	void createDepthCollisionCamera();
	Wolf::ResourceUniqueOwner<Wolf::OrthographicCamera> m_depthCollisionCamera;
	float m_collisionDepthScale;
	float m_collisionDepthOffset;
};
