#extension GL_ARB_separate_shader_objects : enable

struct ParticleInfoGPU
{
	uint createdFrameIdx;
	vec3 position;
	vec3 speed;

	uint emitterIdx;
	float age;
};

const uint MAX_TOTAL_PARTICLES = 262144;
layout(std430, set = 0, binding = 0) restrict buffer ParticleInfoBufferLayout
{
    ParticleInfoGPU particlesInfo[MAX_TOTAL_PARTICLES];
};
 
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
	vec3(0.0f, 0.0f, 0.0f), // bot left
	vec3(1.0f, 0.0f, 0.0f), // bot right
	vec3(0.0f, 1.0f, 0.0f), // top left

	vec3(0.0f, 1.0f, 0.0f), // top left
	vec3(1.0f, 0.0f, 0.0f), // bot right
	vec3(1.0f, 1.0f, 0.0f)  // top right
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

	vec3 worldPos = quadVertices[vertexIdx] + particlesInfo[particleIdx].position;
	vec4 viewPos = getViewMatrix() * vec4(worldPos, 1.0);

    gl_Position = getProjectionMatrix() * viewPos;

	outTexCoord = quadTexCoords[vertexIdx];
	outEmitterIdx = particlesInfo[particleIdx].emitterIdx;
	outTBN = mat3(1.0f);
	outAge = particlesInfo[particleIdx].age;
} 
