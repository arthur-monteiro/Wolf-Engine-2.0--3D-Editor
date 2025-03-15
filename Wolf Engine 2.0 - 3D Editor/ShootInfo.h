#pragma once

#include <glm/glm.hpp>

struct ShootRequest
{
	glm::vec3 startPoint;
	glm::vec3 direction;
	float length;
	float radius;

	uint32_t startTimerInMs;
};
