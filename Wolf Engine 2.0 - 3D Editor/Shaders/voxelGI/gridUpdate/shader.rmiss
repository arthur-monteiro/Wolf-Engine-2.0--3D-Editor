#extension GL_EXT_ray_tracing : require

#include "payloads.glsl"
layout(location = 0) rayPayloadInEXT FirstRayPayload payload;

vec3 computeSunColor(vec3 direction)
{
    return texture(CubeMap, normalize(direction)).xyz;
}

void main()
{
    if (payload.rayIdx == 0)
    {
        vec3 direction = gl_WorldRayDirectionEXT.xyz;

        float NdotL = max(dot(payload.evaluatedNormal, direction), 0.0);
        payload.hit = false;
        payload.radiance = computeSunColor(direction) * NdotL;
    }
    else if (payload.rayIdx == 1)
    {
        payload.isShadowed = false;
    }
}