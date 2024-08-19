layout (location = 0) out vec4 outColor;

layout (location = 0) in vec2 inTexCoords;
layout (location = 1) flat in uint inEmitterIdx;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in float inAge;

#include "particleBuffer.glsl"

vec2 computeTexCoords(in vec2 tileSize, in uint currentTileIdxU32)
{
    return inTexCoords * tileSize + vec2(currentTileIdxU32 % emitterDrawInfo[inEmitterIdx].flipBookSizeX, currentTileIdxU32 / emitterDrawInfo[inEmitterIdx].flipBookSizeY) * tileSize;
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

            //Lo += computeRadianceForLight(V, normal, roughness, F0, albedo, metalness, L, 1.0f /* attenuation */, ubLights.sunLights[i].sunColor.xyz);

            outColor = vec4(accumaledLightmap.xxx, lightmap0.w * opacity);
        }

        //outColor = vec4(lightmap0.rgb, lightmap0.w * opacity);
    }
    else
    {
        vec4 albedo = mix(firstMaterialInfo.albedo, nextMaterialInfo.albedo, currentTileIdx - uint(currentTileIdx));

        outColor = vec4(albedo.rgb, albedo.w * opacity);
    }
}