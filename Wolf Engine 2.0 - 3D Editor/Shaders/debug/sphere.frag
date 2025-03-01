layout (early_fragment_tests) in;

layout(binding = 0, set = 1, std140) uniform readonly UniformBuffer
{
	uint displayType;
} ub;

const uint MAX_SPHERE_COUNT = 128;
layout(binding = 0, set = 2) uniform UniformBufferSphere
{
    vec4 worldPosAndRadius[MAX_SPHERE_COUNT];
    vec4 colors[MAX_SPHERE_COUNT];
} ubSphere;

layout(location = 0) flat in uint inInstanceId;
layout(location = 0) out vec4 outColor;

void main() 
{
    outColor = vec4(ubSphere.colors[inInstanceId].xyz, 1.0);
}