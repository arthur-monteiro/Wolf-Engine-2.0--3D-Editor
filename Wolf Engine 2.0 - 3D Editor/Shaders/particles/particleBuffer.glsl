const uint OPACITY_VALUE_COUNT = 32;
const uint SIZE_VALUE_COUNT = 32;
struct EmitterDrawInfo
{
	uint materialIdx;
    float opacity[OPACITY_VALUE_COUNT];
    float size[SIZE_VALUE_COUNT];

    uint flipBookSizeX;
    uint flipBookSizeY;
};

const uint MAX_EMITTER_COUNT = 16;
layout(std430, set = 0, binding = 1) restrict buffer EmittersInfoBufferLayout
{
    EmitterDrawInfo emitterDrawInfo[MAX_EMITTER_COUNT];
};
