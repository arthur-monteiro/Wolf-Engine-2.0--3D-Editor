#extension GL_EXT_nonuniform_qualifier : enable

const float BIAS = 0.0; // defined in the pipeline
const float SEAM_RANGE = 1.0;
const float HALF_SEAM_RANGE = SEAM_RANGE / 2.0;
const uint CASCADES_COUNT = 4;

const uint NOISE_TEXTURE_SIZE_PER_SIDE = 128;
const uint NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE = 4;
const uint NOISE_TEXTURE_PATTERN_PIXEL_COUNT = NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE * NOISE_TEXTURE_PATTERN_SIZE_PER_SIDE;

#ifdef COMPUTE_SHADER
const uint LOCAL_SIZE = 16;
shared vec2 sharedStablePositionLightSpace[LOCAL_SIZE][LOCAL_SIZE]; 
#endif

layout (binding = 0, set = £CSM_DESCRIPTOR_SLOT) uniform texture2D depthImage;
layout (binding = 1, set = £CSM_DESCRIPTOR_SLOT, std140) uniform UniformBuffer
{
	uvec2 viewportOffset;
    uvec2 viewportSize;

    mat4[CASCADES_COUNT] lightSpaceMatrices;
    vec4 cascadeSplits;
	vec4[CASCADES_COUNT / 2] cascadeScales;
	uvec4 cascadeTextureSize;
	float noiseRotation;
} ub;
layout (binding = 2, set = £CSM_DESCRIPTOR_SLOT) uniform texture2D[] shadowMaps;
layout (binding = 3, set = £CSM_DESCRIPTOR_SLOT) uniform sampler shadowMapsSampler;
layout (binding = 4, set = £CSM_DESCRIPTOR_SLOT) uniform sampler3D noiseTexture;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

vec2 getCascadeScale(in uint cascadeIndex)
{
    if(cascadeIndex % 2 == 0)
        return ub.cascadeScales[cascadeIndex / 2].xy;
    else
        return ub.cascadeScales[cascadeIndex / 2].zw;
}

float computeShadowOcclusionForCascade(in uint cascadeIndex, in vec4 worldPos, in vec2 dStablePositionLightSpace)
{
	vec4 posLightSpace = biasMat * ub.lightSpaceMatrices[cascadeIndex] * vec4(worldPos.xyz, 1.0);
    vec3 projCoords = posLightSpace.xyz / posLightSpace.w;

	vec2 shadowMapDDX = dStablePositionLightSpace.xx * getCascadeScale(cascadeIndex);
    vec2 shadowMapDDY = dStablePositionLightSpace.yy * getCascadeScale(cascadeIndex);
    
	float currentDepth = projCoords.z;
	float shadow = 0.0;

	for(int i = 0; i < float(NOISE_TEXTURE_PATTERN_PIXEL_COUNT); ++i)
	{
		mat2 rotation = mat2(cos(ub.noiseRotation), -sin(ub.noiseRotation),
							 sin(ub.noiseRotation), cos(ub.noiseRotation));

#ifdef COMPUTE_SHADER
		vec2 noise = rotation * (texture(noiseTexture, vec3((gl_GlobalInvocationID.xy) / float(NOISE_TEXTURE_SIZE_PER_SIDE), i / float(NOISE_TEXTURE_PATTERN_PIXEL_COUNT))).rg / ub.cascadeTextureSize[cascadeIndex]);
#else
		vec2 noise = rotation * (texture(noiseTexture, vec3((gl_FragCoord.xy) / float(NOISE_TEXTURE_SIZE_PER_SIDE), i / float(NOISE_TEXTURE_PATTERN_PIXEL_COUNT))).rg / ub.cascadeTextureSize[cascadeIndex]);
#endif
		noise *= 4.0f; // replace by distance with occluder
		noise *= getCascadeScale(cascadeIndex);

		float closestDepth = textureGrad(sampler2D(shadowMaps[cascadeIndex], shadowMapsSampler), projCoords.xy + noise, shadowMapDDX, shadowMapDDY).r;
		shadow += currentDepth - BIAS > closestDepth  ? 0.0 : 1.0;
	}

	shadow /= float(NOISE_TEXTURE_PATTERN_PIXEL_COUNT);

	return shadow;
}

float computeShadowOcclusion(in vec3 viewPos, in vec4 worldPos)
{
    uint cascadeIndex = CASCADES_COUNT;
	for(uint i = 0; i < CASCADES_COUNT; ++i) 
	{
		if(-viewPos.z <= ub.cascadeSplits[i])
		{	
			cascadeIndex = i;
			break;
		}
	}

	if(cascadeIndex >= CASCADES_COUNT)
	{
		return 1.0;
	}

#ifdef COMPUTE_SHADER
    sharedStablePositionLightSpace[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = (biasMat * ub.lightSpaceMatrices[0] * vec4(worldPos.xyz, 1.0)).xy;

    memoryBarrierShared(); // allow every threads to compute stable pos
	barrier();

    uvec2 firstPixelOnQuad = 2 * (gl_LocalInvocationID.xy / 2);
    vec2 dStablePositionLightSpace = sharedStablePositionLightSpace[firstPixelOnQuad.x + 1][firstPixelOnQuad.y + 0] - sharedStablePositionLightSpace[firstPixelOnQuad.x + 0][firstPixelOnQuad.y + 1];
#else
	vec2 dStablePositionLightSpace = vec2(0);
#endif

    float shadow = computeShadowOcclusionForCascade(cascadeIndex, worldPos, dStablePositionLightSpace);

	if(cascadeIndex != CASCADES_COUNT - 1 && ub.cascadeSplits[cascadeIndex] + viewPos.z < HALF_SEAM_RANGE)
	{
		float nextCascadeShadow = computeShadowOcclusionForCascade(cascadeIndex + 1, worldPos, dStablePositionLightSpace);
		shadow = mix(nextCascadeShadow, shadow, (ub.cascadeSplits[cascadeIndex] + viewPos.z) / SEAM_RANGE + 0.5);
	}
	else if(cascadeIndex != 0 && -viewPos.z - ub.cascadeSplits[cascadeIndex - 1] < HALF_SEAM_RANGE)
	{
		float previousCascadeShadow = computeShadowOcclusionForCascade(cascadeIndex - 1, worldPos, dStablePositionLightSpace);
		shadow = mix(previousCascadeShadow, shadow, (-viewPos.z - ub.cascadeSplits[cascadeIndex - 1]) / SEAM_RANGE + 0.5);
	}

	return shadow;
}