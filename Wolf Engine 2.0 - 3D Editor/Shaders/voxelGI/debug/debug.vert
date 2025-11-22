#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in uint inMaterialID;

layout(std430, binding = 0, set = 2) buffer VoxelGridLayout
{
    vec3 values[];
} voxelGrid;

layout(binding = 1, set = 2) uniform UniformBuffer
{
    uint debugPositionFaceId;
} ub;

layout(location = 0) out uint outVoxelIdx;
layout(location = 1) out vec3 outDirection;

const vec3 FIRST_PROBE_POS = vec3(-16.0f, -5.0f, -16.0f);
const float SPACE_BETWEEN_PROBES = 1.0f;
const uint GRID_SIZE = 32;
 
out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
    vec4 localPos = vec4(inPosition * 0.005, 1.0);
    uint voxelIdx = gl_InstanceIndex;

    uint z = voxelIdx / (GRID_SIZE * GRID_SIZE); 
    uint y = (voxelIdx / GRID_SIZE) % GRID_SIZE; 
    uint x = voxelIdx % GRID_SIZE;
    vec3 probeCenter = FIRST_PROBE_POS + vec3(x, y, z) * SPACE_BETWEEN_PROBES + vec3(SPACE_BETWEEN_PROBES) * 0.5;

    vec3 probeWorldPos = probeCenter;
    if (ub.debugPositionFaceId != 0)
    {
        probeWorldPos = voxelGrid.values[voxelIdx * 12 + 6 + ub.debugPositionFaceId - 1];
    }

	vec4 worldPos = localPos + vec4(probeWorldPos, 0.0);
	vec4 viewPos = getViewMatrix() * worldPos;

    gl_Position = getProjectionMatrix() * viewPos;
    outVoxelIdx = voxelIdx;
    outDirection = normalize(inPosition);
} 