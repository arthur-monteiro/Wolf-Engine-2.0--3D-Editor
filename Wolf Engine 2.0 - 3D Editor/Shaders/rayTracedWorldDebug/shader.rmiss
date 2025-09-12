#extension GL_EXT_ray_tracing : require

struct Payload
{
    float hitDistance;
    uint instanceId;
    uint primitiveId;
    vec3 albedo;
};
layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    payload.hitDistance = 1000.0f;
    payload.instanceId = -1;
    payload.primitiveId = -1;
    payload.albedo = vec3(0.1);
}