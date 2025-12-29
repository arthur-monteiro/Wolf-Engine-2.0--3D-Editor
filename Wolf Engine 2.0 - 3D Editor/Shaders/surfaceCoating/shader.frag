layout (early_fragment_tests) in;

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) flat in uint inMaterialID;
layout (location = 4) in mat3 inTBN;
layout (location = 7) in vec3 inWorldSpaceNormal;
layout (location = 8) in vec3 inWorldSpacePos;
layout (location = 9) flat in uint inEntityId;

layout (location = 0) out vec4 outColor;

void main() 
{
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}