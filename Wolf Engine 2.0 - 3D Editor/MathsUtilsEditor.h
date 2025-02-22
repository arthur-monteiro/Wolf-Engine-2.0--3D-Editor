#pragma once

#include <glm/glm.hpp>

void computeRectangleScaleRotationOffsetFromPoints(const glm::vec3& p0, const glm::vec3& s1, const glm::vec3& s2, glm::vec2& outScale, glm::vec3& outRotation, glm::vec3& outOffset);
void computeRectanglePointsFromScaleRotationOffset(const glm::vec2& scale, const glm::vec3& rotation, const glm::vec3& offset, glm::vec3& outP0, glm::vec3& outS1, glm::vec3& outS2);
void computeTransform(const glm::vec3& scale, const glm::vec3& rotation, const glm::vec3& offset, glm::mat4& outTransform);