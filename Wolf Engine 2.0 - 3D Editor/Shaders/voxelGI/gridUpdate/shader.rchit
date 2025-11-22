#extension GL_EXT_ray_tracing : require

#include "payloads.glsl"
layout(location = 0) rayPayloadInEXT FirstRayPayload inPayload;
//layout(location = 1) rayPayloadEXT SecondRayPayload outPayload;

hitAttributeEXT vec2 attribs;

void main()
{
    // TODO: for some reason, the second trace ray (line 21) produces a GPU hang
    // Vertex vertex = computeVertex(gl_InstanceCustomIndexEXT, gl_PrimitiveID, attribs);
    // const vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT; //vec3(gl_ObjectToWorldEXT * vec4(vertex.pos, 1.0));

    // vec3 L = normalize(-ubLights.sunLights[0].sunDirection.xyz);

    // uint rayFlags = gl_RayFlagsOpaqueEXT;
    // uint cullMask = 0xff;
    // float tmin = 0.001;
    // float tmax = 1000.0;
    // traceRaySpecificPayload(rayFlags, cullMask, 2 /*sbtRecordOffset*/, 2 /*sbtRecordStride*/, 1 /*missIndex*/, worldPos.xyz, tmin, L, tmax, 1);

    // if (outPayload.isShadowed)
    // {
    //     inPayload.radiance = vec3(0.0);
    // }
    // else
    // {
    //     uint materialId = getFirstMaterialIdx(gl_InstanceCustomIndexEXT) + vertex.subMeshIdx;

    //     mat3 usedModelMatrix = transpose(inverse(mat3(gl_WorldToObjectEXT)));
    //     vec3 n = normalize(usedModelMatrix * vertex.normal);
    //     vec3 t = normalize(usedModelMatrix * vertex.tangent);
    //     t = normalize(t - dot(t, n) * n);
    //     vec3 b = normalize(cross(t, n));
    //     mat3 TBN = transpose(mat3(t, b, n));

    //     MaterialInfo materialInfo = fetchMaterial(vertex.texCoords, materialId, TBN, worldPos);
    //     vec3 albedo = materialInfo.albedo.xyz;
    //     vec3 normal = materialInfo.normal.xyz;

    //     float NdotL = max(dot(normal, L), 0.0);

    //     inPayload.radiance = albedo * NdotL;
    // }

    if (inPayload.rayIdx == 0)
    {
        inPayload.hit = true;
        inPayload.hitWorldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

        Vertex vertex = computeVertex(gl_InstanceCustomIndexEXT, gl_PrimitiveID, attribs);
        vec3 L = normalize(-ubLights.sunLights[0].sunDirection.xyz);

        uint materialId = getFirstMaterialIdx(gl_InstanceCustomIndexEXT) + vertex.subMeshIdx;

        mat3 usedModelMatrix = transpose(inverse(mat3(gl_WorldToObjectEXT)));
        vec3 n = normalize(usedModelMatrix * vertex.normal);
        vec3 t = normalize(usedModelMatrix * vertex.tangent);
        t = normalize(t - dot(t, n) * n);
        vec3 b = normalize(cross(t, n));
        mat3 TBN = transpose(mat3(t, b, n));

        MaterialInfo materialInfo = fetchMaterial(vertex.texCoords, materialId, TBN, inPayload.hitWorldPos);
        vec3 albedo = materialInfo.albedo.xyz;
        vec3 normal = materialInfo.normal.xyz;

        float NdotL = max(dot(normal, L), 0.0);

        inPayload.radiance = albedo * NdotL;
    }
    else if (inPayload.rayIdx == 1)
    {
        inPayload.isShadowed = true;
    }
}