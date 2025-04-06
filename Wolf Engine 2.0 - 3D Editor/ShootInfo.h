#pragma once

#include <glm/glm.hpp>

struct ShootRequest
{
	glm::vec3 startPoint;
	glm::vec3 direction;
	float length;
	float radius;
	float smokeParticleSize;
	uint32_t materialsCleanedFlags;

	uint32_t startTimerInMs;
};
