#extension GL_ARB_separate_shader_objects : enable

struct ParticleInfoGPU
{
	vec3 position;
	uint createdFrameIdx;

	uint emitterIdx;
	float age;
	float orientationAngle;
	float sizeMultiplier;
};

const uint MAX_TOTAL_PARTICLES = 262144;
layout(std430, set = 0, binding = 0) restrict buffer ParticleInfoBufferLayout
{
    ParticleInfoGPU particlesInfo[MAX_TOTAL_PARTICLES];
};
 
#include "particleBuffer.glsl"

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out uint outEmitterIdx;
layout(location = 2) out mat3 outTBN;
layout(location = 5) out float outAge;

vec3 quadVertices[6] = 
{ 
	vec3(-0.5f, -0.5f, 0.0f), // bot left
	vec3(0.5f, -0.5f, 0.0f), // bot right
	vec3(-0.5f, 0.5f, 0.0f), // top left

	vec3(-0.5f, 0.5f, 0.0f), // top left
	vec3(0.5f, -0.5f, 0.0f), // bot right
	vec3(0.5f, 0.5f, 0.0f)  // top right
};

vec2 quadTexCoords[6] =
{
	vec2(0.0f, 1.0f), // bot left
	vec2(1.0f, 1.0f), // bot right
	vec2(0.0f, 0.0f), // top left

	vec2(0.0f, 0.0f), // top left
	vec2(1.0f, 1.0f), // bot right
	vec2(1.0f, 0.0f)  // top right
};

void main() 
{
	uint particleIdx = gl_VertexIndex / 6;
	uint vertexIdx = gl_VertexIndex % 6;

	if (particlesInfo[particleIdx].createdFrameIdx == 0)
	{
		gl_Position = vec4(0.0);
		return;
	}

	// Size
	float sizeIdxFloat = particlesInfo[particleIdx].age * SIZE_VALUE_COUNT;
    uint firstSizeIdx = uint(trunc(sizeIdxFloat));
    float lerpValue = sizeIdxFloat - float(firstSizeIdx);

    if (firstSizeIdx == SIZE_VALUE_COUNT - 1)
    {
        firstSizeIdx--;
        lerpValue = 1.0f;
    }

    float firstSize = emitterDrawInfo[particlesInfo[particleIdx].emitterIdx].size[firstSizeIdx];
    float nextSize = emitterDrawInfo[particlesInfo[particleIdx].emitterIdx].size[firstSizeIdx + 1];

    float size = mix(firstSize, nextSize, lerpValue) * particlesInfo[particleIdx].sizeMultiplier;

	// Orientation
	mat3 orientationMatrix = mat3(cos(particlesInfo[particleIdx].orientationAngle), - sin(particlesInfo[particleIdx].orientationAngle), 0,
		sin(particlesInfo[particleIdx].orientationAngle), cos(particlesInfo[particleIdx].orientationAngle), 0,
		0, 0, 1);

	vec3 vertex = orientationMatrix * quadVertices[vertexIdx];

	mat3 invViewRot = inverse(mat3(getViewMatrix()));
	vec3 worldPos = particlesInfo[particleIdx].position + invViewRot * vertex * size;

	vec4 viewPos = getViewMatrix() * vec4(worldPos, 1.0);

    gl_Position = getProjectionMatrix() * viewPos;

	outTexCoord = quadTexCoords[vertexIdx];
	outEmitterIdx = particlesInfo[particleIdx].emitterIdx;
	outTBN = mat3(1.0f);
	outAge = particlesInfo[particleIdx].age;
}
