layout (location = 0) out vec4 outColor;

layout (location = 0) in vec2 inTexCoords;
layout (location = 1) flat in uint inEmitterIdx;
layout (location = 2) in mat3 inTBN;
layout (location = 5) in float inAge;

const uint OPACITY_VALUE_COUNT = 32;
struct EmitterDrawInfo
{
	uint materialIdx;
    float opacity[OPACITY_VALUE_COUNT];
};

const uint MAX_EMITTER_COUNT = 16;
layout(std430, set = 0, binding = 1) restrict buffer EmittersInfoBufferLayout
{
    EmitterDrawInfo emitterDrawInfo[MAX_EMITTER_COUNT];
};

void main() 
{
    uint materialIdx = emitterDrawInfo[inEmitterIdx].materialIdx;

    MaterialInfo materialInfo = fetchMaterial(inTexCoords, materialIdx, inTBN, vec3(0.0f) /* world pos (unused) */);

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

    outColor = vec4(materialInfo.albedo.xyz, materialInfo.albedo.w * opacity);
}