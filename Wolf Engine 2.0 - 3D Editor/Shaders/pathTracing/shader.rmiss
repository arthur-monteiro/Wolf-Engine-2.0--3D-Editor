#extension GL_EXT_ray_tracing : require

struct Payload
{
     float hitDistance;
    vec3 normal;
    vec3 albedo;
    float roughness;
    float metalness;
    mat3 TBN;
};
layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    payload.hitDistance = 10000.0f;
}