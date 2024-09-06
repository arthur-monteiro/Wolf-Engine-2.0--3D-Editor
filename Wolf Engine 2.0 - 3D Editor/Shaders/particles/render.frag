layout (location = 0) out vec4 outColor;

layout (location = 0) in vec2 inTexCoords;
layout (location = 1) flat in uint inEmitterIdx;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in float inAge;
layout (location = 6) in vec3 inWorldPos;
layout (location = 7) in vec3 inParticlePos;
layout (location = 8) in float inSize;

#include "particleBuffer.glsl"

vec2 computeTexCoords(in vec2 tileSize, in uint currentTileIdxU32)
{
    return inTexCoords * tileSize + vec2(currentTileIdxU32 % emitterDrawInfo[inEmitterIdx].flipBookSizeX, currentTileIdxU32 / emitterDrawInfo[inEmitterIdx].flipBookSizeY) * tileSize;
}

float raySphereIntersect(vec3 r0, vec3 rd, vec3 s0, float sr) 
{
    // - r0: ray origin
    // - rd: normalized ray direction
    // - s0: sphere center
    // - sr: sphere radius
    // - Returns distance from r0 to first intersecion with sphere,
    //   or -1.0 if no intersection.
    float a = dot(rd, rd);
    vec3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sr * sr);
    if (b*b - 4.0*a*c < 0.0) {
        return -1.0;
    }
    return (-b - sqrt((b*b) - 4.0*a*c))/(2.0*a);
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() 
{
    float opacityIdxFloat = inAge * OPACITY_VALUE_COUNT;
    uint firstOpacityIdx = uint(trunc(opacityIdxFloat));
    float lerpValue = opacityIdxFloat - float(firstOpacityIdx);

    if (firstOpacityIdx == OPACITY_VALUE_COUNT - 1)
    {
        firstOpacityIdx--;
        lerpValue = 1.0f;
    }

    float firstOpacity = emitterDrawInfo[inEmitterIdx].opacity[firstOpacityIdx];
    float nextOpacity = emitterDrawInfo[inEmitterIdx].opacity[firstOpacityIdx + 1];

    float opacity = mix(firstOpacity, nextOpacity, lerpValue);

    uint materialIdx = emitterDrawInfo[inEmitterIdx].materialIdx;

    vec2 tileSize = vec2(1.0f / emitterDrawInfo[inEmitterIdx].flipBookSizeX, 1.0f / emitterDrawInfo[inEmitterIdx].flipBookSizeY);
    uint tileCount = emitterDrawInfo[inEmitterIdx].flipBookSizeX * emitterDrawInfo[inEmitterIdx].flipBookSizeY;
    float currentTileIdx = inAge * tileCount;
    uint currentTileIdxU32 = uint(currentTileIdx);

    MaterialInfo firstMaterialInfo = fetchMaterial(computeTexCoords(tileSize, currentTileIdxU32), materialIdx, inTBN, vec3(0.0f) /* world pos (unused) */);
    MaterialInfo nextMaterialInfo = fetchMaterial(computeTexCoords(tileSize, currentTileIdxU32 + 1), materialIdx, inTBN, vec3(0.0f) /* world pos (unused) */);

    if (firstMaterialInfo.shadingMode == 2) // 6 ways lighting
    {
        vec4 lightmap0 = mix(firstMaterialInfo.sixWaysLightmap0, nextMaterialInfo.sixWaysLightmap0, currentTileIdx - uint(currentTileIdx));
        vec4 lightmap1 = mix(firstMaterialInfo.sixWaysLightmap1, nextMaterialInfo.sixWaysLightmap1, currentTileIdx - uint(currentTileIdx));

        float accumulatedLighting = 0.0f;
        for (uint i = 0; i < ubLights.sunLightsCount; ++i)
        {
            float sunRightComponent = dot(normalize(transpose(getViewMatrix())[0].xyz), ubLights.sunLights[i].sunDirection.xyz);
            float sunUpComponent = dot(normalize(transpose(getViewMatrix())[1].xyz), ubLights.sunLights[i].sunDirection.xyz);
            float sunForwardComponent = dot(normalize(transpose(getViewMatrix())[2].xyz), ubLights.sunLights[i].sunDirection.xyz);

            vec3 sunDirectionComponents = normalize(vec3(sunRightComponent, sunUpComponent, sunForwardComponent));

            float accumaledLightmap = max(sunDirectionComponents.x, 0) * lightmap0.x;
            accumaledLightmap += max(sunDirectionComponents.y, 0) * lightmap0.y;
            accumaledLightmap += max(sunDirectionComponents.z, 0) * lightmap1.z;

            accumaledLightmap += max(-sunDirectionComponents.x, 0) * lightmap1.x;
            accumaledLightmap += max(-sunDirectionComponents.y, 0) * lightmap1.y;
            accumaledLightmap += max(-sunDirectionComponents.z, 0) * lightmap0.z;

            // Compute shadows
            float sumShadows = 0.0f;
            const uint RAY_MARCH_STEP_COUNT = 1;

            vec3 rayOrigin = getCameraPos();
            vec3 rayDirection = normalize(inWorldPos - rayOrigin);
            vec3 sphereCenter = inParticlePos;
            const float RAND_INFLUENCE = 0.2f;
            float randRadius = rand(vec2(gl_FragCoord.x, lightmap0.w)) * RAND_INFLUENCE - RAND_INFLUENCE * 0.5f;
            float sphereRadius = inSize * (lightmap0.w + randRadius);
            float minRayLength = raySphereIntersect(rayOrigin, rayDirection, sphereCenter, sphereRadius);

            if (minRayLength < 0)
            {
                outColor = vec4(0.0);
                return;
            }
            
            for (uint rayMarchStep = 0; rayMarchStep < RAY_MARCH_STEP_COUNT; ++rayMarchStep)
            {
                float maxRayLength = minRayLength + 2.0f * sphereRadius;
                float rayLength = mix(minRayLength, maxRayLength, float(rayMarchStep) / float(RAY_MARCH_STEP_COUNT));

                vec3 worldPos = rayOrigin + rayDirection * rayLength;
                vec3 viewPos = (getViewMatrix() * vec4(worldPos, 1.0)).xyz;

                sumShadows += computeShadowOcclusion(viewPos, vec4(worldPos, 1.0f));
            }

            accumulatedLighting += accumaledLightmap * sumShadows / float(RAY_MARCH_STEP_COUNT);
        }

        outColor = vec4(accumulatedLighting.rrr, lightmap0.w * opacity);
    }
    else
    {
        vec4 albedo = mix(firstMaterialInfo.albedo, nextMaterialInfo.albedo, currentTileIdx - uint(currentTileIdx));

        outColor = vec4(albedo.rgb, albedo.w * opacity);
    }
}