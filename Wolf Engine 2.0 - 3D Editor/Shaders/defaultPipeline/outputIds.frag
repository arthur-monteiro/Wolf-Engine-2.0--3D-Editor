layout (early_fragment_tests) in;

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) flat in uint inMaterialID;
layout (location = 3) in mat3 inTBN;
layout (location = 6) in vec3 inWorldSpaceNormal;
layout (location = 7) in vec3 inWorldSpacePos;
layout (location = 8) flat in uint inEntityId;

layout (location = 0) out uvec4 outIds;

void main() 
{
    outIds = uvec4(inEntityId, 0, 0, 0);
}