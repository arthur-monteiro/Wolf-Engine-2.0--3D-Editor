#ifdef NO_LIGHTING
    #define SURFACE_COATING_DESCRIPTOR_SET_IDX 1
#else
    #define SURFACE_COATING_DESCRIPTOR_SET_IDX 6
#endif

layout (binding = 0, set = SURFACE_COATING_DESCRIPTOR_SET_IDX) uniform texture2D depthTexture;
layout (binding = 1, set = SURFACE_COATING_DESCRIPTOR_SET_IDX) uniform texture2D normalTexture;
layout (binding = 2, set = SURFACE_COATING_DESCRIPTOR_SET_IDX) uniform sampler depthSampler;

layout (binding = 3, set = SURFACE_COATING_DESCRIPTOR_SET_IDX, std140) uniform UniformBuffer
{
	mat4 depthViewProjMatrix;

    float yMin;
    float yMax;
    float verticalOffset;
    float globalThickness;

    uint patternImageCount;
    uint materialIdx;
    vec2 padding;
} ub;

const uint MAX_PATTERN_IMAGES = 4;
layout (binding = 4, set = SURFACE_COATING_DESCRIPTOR_SET_IDX) uniform texture2D[] patternTextures;
layout (binding = 5, set = SURFACE_COATING_DESCRIPTOR_SET_IDX, r32ui) uniform uimage2D patternIdxImage;