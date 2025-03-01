layout (early_fragment_tests) in;

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) flat in uint inMaterialID;
layout (location = 3) in mat3 inTBN;
layout (location = 6) in vec3 inWorldSpaceNormal;
layout (location = 7) in vec3 inWorldSpacePos;

layout (location = 0) out vec4 outColor;

layout(binding = 0, set = 1, std140) uniform readonly UniformBufferDisplay
{
	uint displayType;
} ubDisplay;

layout (binding = 0, set = 4, r32f) uniform image2D shadowMask;

layout (binding = 0, set = 5, std140) uniform readonly uniformBufferCylinderGas
{
    vec3 color;
    float currentValue; 

    float minY;
    float maxY;
    vec2 padding;
} ubCylinderGas;

const uint DISPLAY_TYPE_ALBEDO = 0;
const uint DISPLAY_TYPE_NORMAL = 1;
const uint DISPLAY_TYPE_ROUGHNESS = 2;
const uint DISPLAY_TYPE_METALNESS = 3;
const uint DISPLAY_TYPE_MAT_AO = 4;
const uint DISPLAY_TYPE_MAT_ANISO_STRENGTH = 5;
const uint DISPLAY_TYPE_LIGHTING = 6;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

vec3 computeRadianceForLight(in vec3 V, in vec3 normal, in float roughness, in vec3 F0, in vec3 albedo, in float metalness, in vec3 L, in float attenuation, in vec3 color)
{
    vec3 H = normalize(V + L);
    vec3 radiance = color * attenuation;

    // cook-torrance brdf
    float NDF = DistributionGGX(normal, H, roughness);
    float G   = GeometrySmith(normal, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;

    vec3 nominator    = NDF * G * F;
    float denominator = 4 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0);
    vec3 specular     = nominator / max(denominator, 0.001);

    // add to outgoing radiance Lo
    float NdotL = max(dot(normal, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec4 computeLighting(MaterialInfo materialInfo)
{
    if (materialInfo.shadingMode == 0)
    {
        vec3 albedo = materialInfo.albedo.rgb;
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

        // Per-point-light radiance
        for (uint i = 0; i < ubLights.pointLightsCount; ++i)
        {
            vec3 L = normalize(ubLights.pointLights[i].lightPos.xyz - worldPos);
            vec3 H = normalize(V + L);
            float distance    = length(ubLights.pointLights[i].lightPos.xyz - worldPos);
            float attenuation = 1.0 / (distance * distance); // candela to lux -> lx = cd / (d*d)

            Lo += computeRadianceForLight(V, normal, roughness, F0, albedo, metalness, L, attenuation, ubLights.pointLights[i].lightColor.xyz);
        }

        for (uint i = 0; i < ubLights.sunLightsCount; ++i)
        {
            vec3 L = normalize(-ubLights.sunLights[i].sunDirection.xyz);
            vec3 H = normalize(V + L);

            Lo += computeRadianceForLight(V, normal, roughness, F0, albedo, metalness, L, 1.0f /* attenuation */, ubLights.sunLights[i].sunColor.xyz) * imageLoad(shadowMask, ivec2(gl_FragCoord.xy)).r;
        }

        return vec4(Lo, 1.0);
    }
    else 
    {
        return vec4(1, 0, 0, 1);
    }
}

void main() 
{
    MaterialInfo materialInfo = fetchMaterial(inTexCoords, inMaterialID, inTBN, computeWorldPosFromViewPos(inViewPos));

    float objectSpaceY = (inWorldSpacePos.y - ubCylinderGas.minY) / (ubCylinderGas.maxY - ubCylinderGas.minY);
    if (objectSpaceY < ubCylinderGas.currentValue)
        materialInfo.albedo = vec4(ubCylinderGas.color, 1.0f);
    else
        materialInfo.albedo = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    if (ubDisplay.displayType == DISPLAY_TYPE_ALBEDO)
        outColor = vec4(materialInfo.albedo.rgb, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_NORMAL)
        outColor = vec4(materialInfo.normal, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_ROUGHNESS)
        outColor = vec4(materialInfo.roughness.rrr, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_METALNESS)
        outColor = vec4(materialInfo.metalness.rrr, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_MAT_AO)
        outColor = vec4(materialInfo.matAO.rrr, 1.0);
    else if (ubDisplay.displayType == DISPLAY_TYPE_MAT_ANISO_STRENGTH)
        outColor = vec4(materialInfo.anisoStrength.rrr, 1.0);
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

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
