#extension GL_EXT_ray_tracing : require

hitAttributeEXT vec2 attribs;

#include "payloads.glsl"
layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    payload.hasHit = true;

    Vertex vertex = computeVertex(gl_InstanceCustomIndexEXT, gl_PrimitiveID, attribs);

    uint materialId = getFirstMaterialIdx(gl_InstanceCustomIndexEXT) + vertex.subMeshIdx;

    mat3 usedModelMatrix = transpose(inverse(mat3(gl_WorldToObjectEXT)));
    vec3 n = normalize(usedModelMatrix * vertex.normal);
	vec3 t = normalize(usedModelMatrix * vertex.tangent);
	t = normalize(t - dot(t, n) * n);
	vec3 b = normalize(cross(t, n));
	mat3 TBN = transpose(mat3(t, b, n));

    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(vertex.pos, 1.0));

    MaterialInfo materialInfo = fetchMaterial(vertex.texCoords, materialId, TBN, worldPos);
    payload.albedo = materialInfo.albedo.rgb;
    payload.normal = materialInfo.normal.xyz;
}