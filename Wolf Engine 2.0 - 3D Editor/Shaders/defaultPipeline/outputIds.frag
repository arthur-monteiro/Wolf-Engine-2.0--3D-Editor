layout (early_fragment_tests) in;

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) flat in uint inMaterialID;
layout (location = 3) in mat3 inTBN;
layout (location = 6) in vec3 inWorldSpaceNormal;
layout (location = 7) in vec3 inWorldSpacePos;
layout (location = 8) in uint inEntityId;

layout (location = 0) out uvec4 outIds;

layout(binding = 0, set = 1, std140) uniform readonly UniformBufferDisplay
{
	uint displayType;
} ubDisplay;

layout (binding = 0, set = 4, r32f) uniform image2D shadowMask;

const uint DISPLAY_TYPE_ALBEDO = 0;
const uint DISPLAY_TYPE_NORMAL = 1;
const uint DISPLAY_TYPE_ROUGHNESS = 2;
const uint DISPLAY_TYPE_METALNESS = 3;
const uint DISPLAY_TYPE_MAT_AO = 4;
const uint DISPLAY_TYPE_MAT_ANISO_STRENGTH = 5;
const uint DISPLAY_TYPE_LIGHTING = 6;

void main() 
{
    outIds = uvec4(inEntityId, 0, 0, 0);
}