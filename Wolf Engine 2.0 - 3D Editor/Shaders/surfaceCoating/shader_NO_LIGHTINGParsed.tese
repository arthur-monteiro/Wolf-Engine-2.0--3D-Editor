#version 460
#define GLSL

#define NO_LIGHTING


layout(binding = 0, set = 0) uniform UniformBufferCamera
{
	mat4 view;

	mat4 projection;

	mat4 invView;

	mat4 invProjection;

	mat4 previousViewMatrix;

	vec2 jitter;
	vec2 projectionParams;

	float near;
	float far;
	uint  frameIndex;
	uint  extentWidth;
} ubCamera;

mat4 getViewMatrix()
{
	return ubCamera.view;
}

mat4 getProjectionMatrix()
{
	return ubCamera.projection;
}

mat4 getInvViewMatrix()
{
	return ubCamera.invView;
}

mat4 getInvProjectionMatrix()
{
	return ubCamera.invProjection;
}

vec2 getProjectionParams()
{
	return ubCamera.projectionParams;
}

vec2 getCameraJitter()
{
	return ubCamera.jitter;
}

float linearizeDepth(float d)
{
    return ubCamera.near * ubCamera.far / (ubCamera.far - d * (ubCamera.far - ubCamera.near));
}

mat4 getPreviousViewMatrix()
{
	return ubCamera.previousViewMatrix;
}

vec3 computeWorldPosFromViewPos(in const vec3 viewPos)
{
	return (getInvViewMatrix() * vec4(viewPos, 1.0)).xyz;
}

vec3 getCameraPos()
{
	return ubCamera.invView[3].xyz;
}

uint getCameraFrameIndex()
{
	return ubCamera.frameIndex;
}

uint getScreenWidth()
{
	return ubCamera.extentWidth;
}
#extension GL_EXT_nonuniform_qualifier : enable

layout(quads, equal_spacing, ccw) in;

layout(location = 0) flat in uint inEntityId[];
layout(location = 1) in vec2 inGridUVs[];

layout(location = 0) flat out uint outEntityId;
layout(location = 1) out vec3 outNormal;

#include "resources.glsl"

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() 
{
    // Compute input data
    vec4 p1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
    vec4 p2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.x);  

    vec4 worldPos = mix(p1, p2, gl_TessCoord.y);
    
    // Ajust position to geometry (with depth texture)
    vec4 posProjectionSpace = biasMat * ub.depthViewProjMatrix * vec4(worldPos.xyz, 1.0);
    vec3 projCoords = posProjectionSpace.xyz / posProjectionSpace.w;

    float depth = texture(sampler2D(depthTexture, depthSampler), projCoords.xy).r;
    if (depth > 0.999)
    {
        gl_Position = vec4(0.0 / 0.0);
        return;
    }
    vec3 normal = texture(sampler2D(normalTexture, depthSampler), projCoords.xy).xyz;

    float yPos = mix(ub.yMin, ub.yMax, 1.0 - depth);
    worldPos.y = yPos + ub.verticalOffset;

    // Apply patter
    vec2 gridUVs1 = mix(inGridUVs[0], inGridUVs[1], gl_TessCoord.x);
    vec2 gridUVs2 = mix(inGridUVs[2], inGridUVs[3], gl_TessCoord.x);  
    vec2 gridUVs = mix(gridUVs1, gridUVs2, gl_TessCoord.y);

    vec2 patternIdxPixelCoord = gridUVs * vec2(32.0 /* grid size */) - vec2(0.5);
    ivec2 patternIdxPixelICoord = ivec2(floor(patternIdxPixelCoord));
    vec2 patternIdxLerpCoeffs = fract(patternIdxPixelCoord);
    
    uint patternIdces[4];
    patternIdces[0] = (imageLoad(patternIdxImage, patternIdxPixelICoord + ivec2(0, 0)).r * ub.patternImageCount) / (MAX_PATTERN_IMAGES + 1); // Bottom-Left
    patternIdces[1] = (imageLoad(patternIdxImage, patternIdxPixelICoord + ivec2(1, 0)).r * ub.patternImageCount) / (MAX_PATTERN_IMAGES + 1); // Bottom-Right
    patternIdces[2] = (imageLoad(patternIdxImage, patternIdxPixelICoord + ivec2(0, 1)).r * ub.patternImageCount) / (MAX_PATTERN_IMAGES + 1); // Top-Left
    patternIdces[3] = (imageLoad(patternIdxImage, patternIdxPixelICoord + ivec2(1, 1)).r * ub.patternImageCount) / (MAX_PATTERN_IMAGES + 1); // Top-Right

    float patternHeights[4];
    vec3 patternNormals[4];
    for (uint i = 0; i < 4; ++i)
    {
        patternHeights[i] = texture(sampler2D(patternTextures[nonuniformEXT(patternIdces[i] * 2)], depthSampler), worldPos.xz * 0.5).r;
        patternNormals[i] = texture(sampler2D(patternTextures[nonuniformEXT(patternIdces[i] * 2 + 1)], depthSampler), worldPos.xz * 0.5).xyz;
    }

    float patternHeight1 = mix(patternHeights[0], patternHeights[1], patternIdxLerpCoeffs.x);
    float patternHeight2 = mix(patternHeights[2], patternHeights[3], patternIdxLerpCoeffs.x);
    float patternHeight = mix(patternHeight1, patternHeight2, patternIdxLerpCoeffs.y);

    vec3 patternNormal1 = mix(patternNormals[0], patternNormals[1], patternIdxLerpCoeffs.x);
    vec3 patternNormal2 = mix(patternNormals[2], patternNormals[3], patternIdxLerpCoeffs.x);
    vec3 patternNormal = mix(patternNormal1, patternNormal2, patternIdxLerpCoeffs.y);
    patternNormal.xy *= vec2(ub.globalThickness);
    patternNormal = normalize(patternNormal);

    worldPos.y += patternHeight * ub.globalThickness;

    vec3 n = normal;
    vec3 t = vec3(1, 0, 0);
    vec3 b = normalize(cross(t, n));
    mat3 TBN = transpose(mat3(t, b, n));
    normal = patternNormal * TBN;

    // Output data
    gl_Position = vec4(worldPos.xyz, 1.0f);

    outEntityId = inEntityId[0];
    outNormal = normal;
}
