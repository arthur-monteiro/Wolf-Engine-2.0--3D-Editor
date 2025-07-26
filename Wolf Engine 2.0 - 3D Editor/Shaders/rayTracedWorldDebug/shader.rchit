#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float hitDistance;

void main()
{
    hitDistance = gl_HitTEXT;

    int instanceId = gl_InstanceID;
}