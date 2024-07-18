#extension GL_EXT_nonuniform_qualifier : enable

layout (binding = 0, set = £BINDLESS_DESCRIPTOR_SLOT) uniform texture2D[] textures;
layout (binding = 1, set = £BINDLESS_DESCRIPTOR_SLOT) uniform sampler textureSampler;

struct InputMaterialInfo
{
    uint albedoIdx;
	uint normalIdx;
	uint roughnessMetalnessAOIdx;

    uint shadingMode;
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
    float anisoStrength;
	
	uint shadingMode;
};

uint fetchContamination(in const vec3 worldPos)
{
    vec3 sampleUV = (worldPos - offset) / size;
    if (any(lessThan(sampleUV, vec3(0.0))) || any(greaterThan(sampleUV, vec3(1.0))))
        return 0;
    return uint(texture(contaminationIds, sampleUV).r);
}

MaterialInfo internalFetch(in const vec2 texCoords, in uint materialId, in const mat3 matrixTBN)
{
    MaterialInfo materialInfo;

    materialInfo.albedo = texture(sampler2D(textures[materialsInfo[materialId].albedoIdx], textureSampler), texCoords).rgb;
    materialInfo.normal = (texture(sampler2D(textures[materialsInfo[materialId].normalIdx], textureSampler), texCoords).rgb * 2.0 - vec3(1.0)) * matrixTBN;
    vec4 combinedRoughnessMetalnessAOAniso = texture(sampler2D(textures[materialsInfo[materialId].roughnessMetalnessAOIdx], textureSampler), texCoords).rgba;
	materialInfo.roughness = combinedRoughnessMetalnessAOAniso.r;
	materialInfo.metalness = combinedRoughnessMetalnessAOAniso.g;
    materialInfo.matAO = combinedRoughnessMetalnessAOAniso.b;
    materialInfo.anisoStrength = combinedRoughnessMetalnessAOAniso.a;
	materialInfo.shadingMode = materialsInfo[materialId].shadingMode;

    return materialInfo;
}

MaterialInfo fetchMaterial(in const vec2 texCoords, in uint materialId, in const mat3 matrixTBN, in const vec3 worldPos)
{
    uint contaminationId = fetchContamination(worldPos);
    if (contaminationId > 0)
    {
        materialId = materialOffsets[contaminationId - 1];

        vec3 normal = abs(vec3(matrixTBN[0][2], matrixTBN[1][2], matrixTBN[2][2]));

        // normalized total value to 1.0
        float b = (normal.x + normal.y + normal.z);
        normal /= b;

        MaterialInfo xAxis = internalFetch(worldPos.yz, materialId, matrixTBN);
        MaterialInfo yAxis = internalFetch(worldPos.xz, materialId, matrixTBN);
        MaterialInfo zAxis = internalFetch(worldPos.xy, materialId, matrixTBN);

        MaterialInfo finalMaterialInfo;
#define LERP_INFO(name) (xAxis.name * normal.x + yAxis.name * normal.y + zAxis.name * normal.z);
        finalMaterialInfo.albedo = LERP_INFO(albedo);
        finalMaterialInfo.normal = LERP_INFO(normal);
        finalMaterialInfo.roughness = LERP_INFO(roughness);
        finalMaterialInfo.metalness = LERP_INFO(metalness);
        finalMaterialInfo.matAO = LERP_INFO(matAO);
		finalMaterialInfo.shadingMode = xAxis.shadingMode;

        return finalMaterialInfo;
    }
    else
    {
        return internalFetch(texCoords, materialId, matrixTBN);
    }
}