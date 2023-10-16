#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout (early_fragment_tests) in;

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) flat in uint inMaterialID;
layout (location = 3) in mat3 inTBN;
layout (location = 6) in vec3 inWorldSpaceNormal;
layout (location = 7) in vec3 inWorldSpacePos;

layout (location = 0) out vec4 outColor;

layout (binding = 0, set = 0) uniform texture2D[] textures;
layout (binding = 1, set = 0) uniform sampler textureSampler;

void main() 
{
    vec3 albedo = texture(sampler2D(textures[inMaterialID * 5     + 0], textureSampler), inTexCoords).rgb;

    outColor = vec4(albedo, 1.0);
}