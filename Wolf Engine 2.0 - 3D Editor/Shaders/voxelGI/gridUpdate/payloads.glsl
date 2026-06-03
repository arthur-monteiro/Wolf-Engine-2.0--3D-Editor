struct FirstRayPayload
{
    uint rayIdx;

    // First ray
    bool hit;
    vec3 hitWorldPos;
    vec3 hitNormal;
    vec3 hitAlbedo;
    vec3 radiance;

    // Second ray
    bool isShadowed;
};

struct SecondRayPayload
{
    bool isShadowed;
};