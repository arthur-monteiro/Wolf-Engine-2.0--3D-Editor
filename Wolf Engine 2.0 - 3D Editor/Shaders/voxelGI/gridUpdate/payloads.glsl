struct FirstRayPayload
{
    uint rayIdx;

    // First ray
    bool hit;
    vec3 hitWorldPos;
    vec3 evaluatedNormal;
    vec3 radiance;

    // Second ray
    bool isShadowed;
};

struct SecondRayPayload
{
    bool isShadowed;
};