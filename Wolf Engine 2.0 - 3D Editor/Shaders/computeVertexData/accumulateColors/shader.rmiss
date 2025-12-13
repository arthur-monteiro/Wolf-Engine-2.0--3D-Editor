#extension GL_EXT_ray_tracing : require

#include "payloads.glsl"
layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    payload.hasHit = false;
}