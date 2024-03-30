#extension GL_EXT_nonuniform_qualifier : enable

layout (binding = 0, set = £BINDLESS_DESCRIPTOR_SLOT) uniform texture2D[] textures;
layout (binding = 1, set = £BINDLESS_DESCRIPTOR_SLOT) uniform sampler textureSampler;

struct InputMaterialInfo
{
    uint albedoIdx;
	uint normalIdx;
	uint roughnessMetalnessAOIdx;
};

layout(std430, binding = 2, set = £BINDLESS_DESCRIPTOR_SLOT) readonly restrict buffer MaterialBufferLayout
{
    InputMaterialInfo materialsInfo[];
};

layout (binding = 0, set = £CONTAMINATION_DESCRIPTOR_SLOT) uniform usampler3D contaminationIds;
const uint MAX_MATERIALS = 8;
layout(std430, binding = 1, set = £CONTAMINATION_DESCRIPTOR_SLOT) readonly restrict buffer contaminationInfo
{
    vec3 offset;
    float size;
    uint materialOffsets[MAX_MATERIALS];
};

struct MaterialInfo
{
    vec3 albedo;
    vec3 normal;
    float roughness;
    float metalness;
    float matAO;
};

uint fetchContamination(in const vec3 worldPos)
{
    vec3 sampleUV = (worldPos - offset) / size;
    if (any(lessThan(sampleUV, vec3(0.0))) || any(greaterThan(sampleUV, vec3(1.0))))
        return 0;
    return uint(texture(contaminationIds, sampleUV).r);
}

MaterialInfo fetchMaterial(in const vec2 texCoords, in uint materialId, in const mat3 matrixTBN, in const vec3 worldPos)
{
    uint contaminationId = fetchContamination(worldPos);
    if (contaminationId > 0)
        materialId = materialOffsets[contaminationId - 1];

    MaterialInfo materialInfo;

    materialInfo.albedo = texture(sampler2D(textures[materialsInfo[materialId].albedoIdx], textureSampler), texCoords).rgb;
    materialInfo.normal = (texture(sampler2D(textures[materialsInfo[materialId].normalIdx], textureSampler), texCoords).rgb * 2.0 - vec3(1.0)) * matrixTBN;
    vec3 combinedRoughnessMetalnessAO = texture(sampler2D(textures[materialsInfo[materialId].roughnessMetalnessAOIdx], textureSampler), texCoords).rgb;
	materialInfo.roughness = combinedRoughnessMetalnessAO.r;
	materialInfo.metalness = combinedRoughnessMetalnessAO.g;
    materialInfo.matAO = combinedRoughnessMetalnessAO.b;

    return materialInfo;
}