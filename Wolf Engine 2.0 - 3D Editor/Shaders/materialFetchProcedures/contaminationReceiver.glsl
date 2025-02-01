layout (binding = 0, set = £CONTAMINATION_DESCRIPTOR_SLOT) uniform usampler3D contaminationIds;
const uint MAX_MATERIALS = 8;
layout(std430, binding = 1, set = £CONTAMINATION_DESCRIPTOR_SLOT) readonly restrict buffer contaminationInfo
{
    vec3 offset;
    float size;
    float cellSize;
    uint materialIds[MAX_MATERIALS];
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
    {
        materialId = materialIds[contaminationId - 1];

        vec3 normal = abs(vec3(matrixTBN[0][2], matrixTBN[1][2], matrixTBN[2][2]));

        // normalized total value to 1.0
        float b = (normal.x + normal.y + normal.z);
        normal /= b;

        MaterialInfo xAxis = defaultFetchMaterial(worldPos.yz, materialId, matrixTBN, worldPos);
        MaterialInfo yAxis = defaultFetchMaterial(worldPos.xz, materialId, matrixTBN, worldPos);
        MaterialInfo zAxis = defaultFetchMaterial(worldPos.xy, materialId, matrixTBN, worldPos);

        MaterialInfo finalMaterialInfo;
#define LERP_INFO_CUSTOM(name) (xAxis.name * normal.x + yAxis.name * normal.y + zAxis.name * normal.z);
        finalMaterialInfo.albedo = LERP_INFO_CUSTOM(albedo);
        finalMaterialInfo.normal = LERP_INFO_CUSTOM(normal);
        finalMaterialInfo.roughness = LERP_INFO_CUSTOM(roughness);
        finalMaterialInfo.metalness = LERP_INFO_CUSTOM(metalness);
        finalMaterialInfo.matAO = LERP_INFO_CUSTOM(matAO);
		finalMaterialInfo.shadingMode = xAxis.shadingMode;

        return finalMaterialInfo;
    }
    else
    {
        return defaultFetchMaterial(texCoords, materialId, matrixTBN, worldPos);
    }
}