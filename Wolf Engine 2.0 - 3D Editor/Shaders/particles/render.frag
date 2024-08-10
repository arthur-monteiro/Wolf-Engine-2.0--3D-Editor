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
    uint materialIdx = emitterDrawInfo[inEmitterIdx].materialIdx;

    vec2 tileSize = vec2(1.0f / emitterDrawInfo[inEmitterIdx].flipBookSizeX, 1.0f / emitterDrawInfo[inEmitterIdx].flipBookSizeY);
    uint tileCount = emitterDrawInfo[inEmitterIdx].flipBookSizeX * emitterDrawInfo[inEmitterIdx].flipBookSizeY;
    float currentTileIdx = inAge * tileCount;
    uint currentTileIdxU32 = uint(currentTileIdx);

    MaterialInfo firstMaterialInfo = fetchMaterial(computeTexCoords(tileSize, currentTileIdxU32), materialIdx, inTBN, vec3(0.0f) /* world pos (unused) */);
    MaterialInfo nextMaterialInfo = fetchMaterial(computeTexCoords(tileSize, currentTileIdxU32 + 1), materialIdx, inTBN, vec3(0.0f) /* world pos (unused) */);

    vec4 albedo = mix(firstMaterialInfo.albedo, nextMaterialInfo.albedo, currentTileIdx - uint(currentTileIdx));

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

    outColor = vec4(albedo.xyz, albedo.w * opacity);
}