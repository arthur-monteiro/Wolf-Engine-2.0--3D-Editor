#include "MathsUtilsEditor.h"

#include <glm/gtc/matrix_transform.hpp>

void computeRectangleScaleRotationOffsetFromPoints(const glm::vec3& p0, const glm::vec3& s1, const glm::vec3& s2, glm::vec2& outScale, glm::vec3& outRotation, glm::vec3& outOffset)
{
	glm::vec3 p1 = p0 + s1;
	glm::vec3 p2 = p0 + s2;
	glm::vec3 p3 = p0 + s1 + s2;

	outOffset = ((p0 + p1 + p2 + p3) * (1.0f / 4.0f)) - glm::vec3(0.0f); // center rectangle - center default square

	glm::mat3 squareBasis(1.0f);
	glm::mat3 inputBasis(glm::normalize(s1), glm::normalize(s2), glm::cross(glm::normalize(s1), glm::normalize(s2)));
	glm::mat3 rotationMatrix = inputBasis * glm::transpose(squareBasis);
	outRotation.y = glm::asin(rotationMatrix[0][2]);
	if (abs(outRotation.y) < 1.0f)
	{
		outRotation.x = std::atan2(-rotationMatrix[1][2], rotationMatrix[2][2]);
		outRotation.z = std::atan2(-rotationMatrix[0][1], rotationMatrix[0][0]);
	}
	else
	{
		outRotation.x = std::atan2(rotationMatrix[2][1], rotationMatrix[1][1]);
		outRotation.z = 0;
	}

	outScale.x = glm::length(s1);
	outScale.y = glm::length(s2);
}

void computeRectanglePointsFromScaleRotationOffset(const glm::vec2& scale, const glm::vec3& rotation, const glm::vec3& offset, glm::vec3& outP0, glm::vec3& outS1, glm::vec3& outS2)
{
	glm::vec3 scale3 = glm::vec3(scale, 1.0f);

	glm::vec3 p0 = glm::vec3(-0.5f, -0.5f, 0.0f) * scale3;
	glm::vec3 p1 = glm::vec3(0.5f, -0.5f, 0.0f) * scale3;
	glm::vec3 p2 = glm::vec3(-0.5f, 0.5f, 0.0f) * scale3;

	glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	rotationMat = glm::rotate(rotationMat, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	rotationMat = glm::rotate(rotationMat, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

	p0 = glm::vec4(p0, 1.0f) * rotationMat;
	p1 = glm::vec4(p1, 1.0f) * rotationMat;
	p2 = glm::vec4(p2, 1.0f) * rotationMat;

	p0 += offset;
	p1 += offset;
	p2 += offset;

	outP0 = p0;
	outS1 = p1 - p0;
	outS2 = p2 - p0;
}

void computeTransform(const glm::vec3& scale, const glm::vec3& rotation, const glm::vec3& offset, glm::mat4& outTransform)
{
	outTransform = glm::translate(glm::mat4(1.0f), offset);
	outTransform = glm::rotate(outTransform, static_cast<glm::vec3>(rotation).x, glm::vec3(1.0f, 0.0f, 0.0f));
	outTransform = glm::rotate(outTransform, static_cast<glm::vec3>(rotation).y, glm::vec3(0.0f, 1.0f, 0.0f));
	outTransform = glm::rotate(outTransform, static_cast<glm::vec3>(rotation).z, glm::vec3(0.0f, 0.0f, 1.0f));
	outTransform = glm::scale(outTransform, static_cast<glm::vec3>(scale));
}

void computeTransformFromTwoPoints(const glm::vec3& p0, const glm::vec3& p1, glm::mat4& outTransform)
{
	glm::vec3 d = glm::normalize(p1 - p0);

	float alpha = -glm::atan(d.z / d.y);
	float beta = glm::atan(d.x / glm::sqrt(d.y * d.y + d.z * d.z));
	if (d.y < 0)
	{
		beta = glm::pi<float>() - beta;
	}

	glm::mat3 rx = glm::mat3(1, 0, 0,
		0, glm::cos(alpha), glm::sin(alpha),
		0, -glm::sin(alpha), glm::cos(alpha));

	glm::mat3 rz(glm::cos(beta), glm::sin(beta), 0,
		-glm::sin(beta), glm::cos(beta), 0,
		0, 0, 1);

	glm::mat4 t = glm::translate(glm::mat4(1.0f), p0);

	outTransform = t * glm::mat4(glm::transpose(rz * rx));
}
