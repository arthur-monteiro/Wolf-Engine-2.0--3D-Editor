#extension GL_EXT_ray_tracing : require

struct Payload
{
    vec3 radiance;
};
layout(location = 0) rayPayloadInEXT Payload payload;

vec3 computeSunColor(vec3 direction)
{
    return texture(CubeMap, normalize(direction)).xyz;
}

void main()
{
    vec3 direction = gl_WorldRayDirectionEXT.xyz;

    payload.radiance = computeSunColor(direction);
}