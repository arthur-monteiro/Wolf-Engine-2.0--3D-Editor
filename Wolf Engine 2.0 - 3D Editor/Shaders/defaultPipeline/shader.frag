layout (early_fragment_tests) in;

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) flat in uint inMaterialID;
layout (location = 3) in mat3 inTBN;
layout (location = 6) in vec3 inWorldSpaceNormal;
layout (location = 7) in vec3 inWorldSpacePos;

layout (location = 0) out vec4 outColor;

layout(binding = 0, set = 2, std140) uniform readonly UniformBufferDisplay
{
	uint displayType;
} ubDisplay;

struct PointLightInfo
{
    vec4 lightPos;
    vec4 lightColor;
};
const uint MAX_POINT_LIGHTS = 16;
layout(binding = 1, set = 2, std140) uniform readonly UniformBufferPointLights
{
    PointLightInfo lights[MAX_POINT_LIGHTS];
    uint count;
} ubPointLights;

const uint DISPLAY_TYPE_ALBEDO = 0;
const uint DISPLAY_TYPE_NORMAL = 1;
const uint DISPLAY_TYPE_ROUGHNESS = 2;
const uint DISPLAY_TYPE_METALNESS = 3;
const uint DISPLAY_TYPE_MAT_AO = 4;
const uint DISPLAY_TYPE_LIGHTING = 5;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

vec4 computeLighting(MaterialInfo materialInfo)
{
    vec3 albedo = materialInfo.albedo;
	vec3 normal = materialInfo.normal;
	float roughness = materialInfo.roughness;
	float metalness = materialInfo.metalness;
    normal = normalize(normal);
    vec3 worldPos = (getInvViewMatrix() * vec4(inViewPos, 1.0f)).xyz;

    vec3 camPos = getInvViewMatrix()[3].xyz;
    vec3 V = normalize(camPos - worldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0,albedo, metalness);

    vec3 Lo = vec3(0.0);

    // calculate per-light radiance
    for (uint i = 0; i < ubPointLights.count; ++i)
    {
        vec3 L = normalize(ubPointLights.lights[i].lightPos.xyz - worldPos);
        vec3 H = normalize(V + L);
        float distance    = length(ubPointLights.lights[i].lightPos.xyz - worldPos);
        float attenuation = 1.0 / (distance * distance); // candela to lux -> lx = cd / (d*d)
        vec3 radiance = ubPointLights.lights[i].lightColor.xyz * attenuation;

        // cook-torrance brdf
        float NDF = DistributionGGX(normal, H, roughness);
        float G   = GeometrySmith(normal, V, L, roughness);
        vec3 F    = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metalness;

        vec3 nominator    = NDF * G * F;
        float denominator = 4 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0);
        vec3 specular     = nominator / max(denominator, 0.001);

        // add to outgoing radiance Lo
        float NdotL = max(dot(normal, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    return vec4(Lo, 1.0);
}

void main() 
{
    MaterialInfo materialInfo = fetchMaterial(inTexCoords, inMaterialID, inTBN, computeWorldPosFromViewPos(inViewPos));

    if (ubDisplay.displayType == DISPLAY_TYPE_ALBEDO)
        outColor = vec4(materialInfo.albedo, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_NORMAL)
        outColor = vec4(materialInfo.normal, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_ROUGHNESS)
        outColor = vec4(materialInfo.roughness.rrr, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_METALNESS)
        outColor = vec4(materialInfo.metalness.rrr, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_MAT_AO)
        outColor = vec4(materialInfo.matAO.rrr, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_LIGHTING)
        outColor = computeLighting(materialInfo);
    else
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
}


float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}
