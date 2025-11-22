#extension GL_EXT_ray_tracing : require

#include "payloads.glsl"
layout(location = 1) rayPayloadInEXT SecondRayPayload payload;

void main()
{
    payload.isShadowed = true;
}