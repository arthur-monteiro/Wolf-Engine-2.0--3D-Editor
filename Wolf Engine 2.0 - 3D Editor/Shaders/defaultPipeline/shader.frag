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

layout(binding = 0, set = 2, std140) uniform readonly UniformBuffer
{
	uint displayType;
} ub;

const uint DISPLAY_TYPE_ALBEDO = 0;
const uint DISPLAY_TYPE_NORMAL = 1;
const uint DISPLAY_TYPE_ROUGHNESS = 2;
const uint DISPLAY_TYPE_METALNESS = 3;
const uint DISPLAY_TYPE_MAT_AO = 4;

void main() 
{
    vec3 albedo = texture(sampler2D(textures[inMaterialID * 5     + 0], textureSampler), inTexCoords).rgb;
    vec3 normal = (texture(sampler2D(textures[inMaterialID * 5    + 1], textureSampler), inTexCoords).rgb * 2.0 - vec3(1.0)) * inTBN;
	float roughness = texture(sampler2D(textures[inMaterialID * 5 + 2], textureSampler), inTexCoords).r;
	float metalness = texture(sampler2D(textures[inMaterialID * 5 + 3], textureSampler), inTexCoords).r;
    float matAO = texture(sampler2D(textures[inMaterialID * 5     + 4], textureSampler), inTexCoords).r;

    if (ub.displayType == DISPLAY_TYPE_ALBEDO)
        outColor = vec4(albedo, 1.0);
    else if (ub.displayType == DISPLAY_TYPE_NORMAL)
        outColor = vec4(normal, 1.0);
    else if (ub.displayType == DISPLAY_TYPE_ROUGHNESS)
        outColor = vec4(roughness.rrr, 1.0);
    else if (ub.displayType == DISPLAY_TYPE_METALNESS)
        outColor = vec4(metalness.rrr, 1.0);
    else if (ub.displayType == DISPLAY_TYPE_MAT_AO)
        outColor = vec4(matAO.rrr, 1.0);
    else 
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
}