#version 460
#define GLSL



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

	vec4 frustumPlanes[6]; // left, right, bottom, top, near, far
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

vec4 getFrustumPlane(uint idx)
{
	return ubCamera.frustumPlanes[idx];
}
layout (vertices = 4) out;

layout(location = 0) flat in uint inEntityId[];
layout(location = 1) in vec2 inGridUVs[];

layout(location = 0) flat out uint outEntityId[];
layout(location = 1) out vec2 outGridUVs[];

#include "resources.glsl"

// https://ktstephano.github.io/rendering/stratusgfx/aabbs
bool isAabbVisible(in vec3 aabbMin, in vec3 aabbMax, in vec4 frustumPlanes[6]) 
{
    for (uint i = 0; i < 6; ++i)
    {
        vec4 plane = frustumPlanes[i];
        if ((dot(plane, vec4(aabbMin.x, aabbMin.y, aabbMin.z, 1.0)) < 0.0) &&
            (dot(plane, vec4(aabbMax.x, aabbMin.y, aabbMin.z, 1.0)) < 0.0) &&
            (dot(plane, vec4(aabbMin.x, aabbMax.y, aabbMin.z, 1.0)) < 0.0) &&
            (dot(plane, vec4(aabbMax.x, aabbMax.y, aabbMin.z, 1.0)) < 0.0) &&
            (dot(plane, vec4(aabbMin.x, aabbMin.y, aabbMax.z, 1.0)) < 0.0) &&
            (dot(plane, vec4(aabbMax.x, aabbMin.y, aabbMax.z, 1.0)) < 0.0) &&
            (dot(plane, vec4(aabbMin.x, aabbMax.y, aabbMax.z, 1.0)) < 0.0) &&
            (dot(plane, vec4(aabbMax.x, aabbMax.y, aabbMax.z, 1.0)) < 0.0))
        {
            return false;
        }
    }

    return true;
}

const float TARGET_PIXELS_PER_EDGE = 12.0;

void main() 
{
    vec2 patchBoundsUVs = inGridUVs[gl_InvocationID].xy;
    ivec2 patchBoundsXY = ivec2(patchBoundsUVs * vec2(32, 32));
    vec2 patchDepthBounds = imageLoad(patchBoundsImage, patchBoundsXY).xy;
    float patchWorldHeightMin = (1.0 - patchDepthBounds.y) * ub.depthScale + ub.depthOffset + ub.verticalOffset;
    float patchWorldHeightMax = (1.0 - patchDepthBounds.x) * ub.depthScale + ub.depthOffset + ub.verticalOffset + ub.globalThickness;

    float minHeight = patchWorldHeightMin;
    float maxHeight = patchWorldHeightMax;

    float tl = 16.0;
    if (gl_InvocationID == 0)
    {       
        vec3 aabbMin = vec3(gl_in[0].gl_Position.x, minHeight, gl_in[0].gl_Position.z);
        vec3 aabbMax = vec3(gl_in[3].gl_Position.x, maxHeight, gl_in[3].gl_Position.z);

        if (!isAabbVisible(aabbMin, aabbMax, ubCamera.frustumPlanes))
        {
            gl_TessLevelOuter[0] = 0;
            gl_TessLevelOuter[1] = 0;
            gl_TessLevelOuter[2] = 0;
            gl_TessLevelOuter[3] = 0;
            gl_TessLevelInner[0] = 0;
            gl_TessLevelInner[1] = 0;
            return;
        }

        float distanceWithPatch = max(distance(getCameraPos(), (aabbMax + aabbMin) * 0.5), 1.0);

        const float minDist = 1.0;
        const float maxDist = 32.0;
        float t = smoothstep(minDist, maxDist, distanceWithPatch);
        float tessellationFactor = mix(32.0, 4.0, t);

        gl_TessLevelOuter[0] = tessellationFactor;
        gl_TessLevelOuter[1] = tessellationFactor;
        gl_TessLevelOuter[2] = tessellationFactor;
        gl_TessLevelOuter[3] = tessellationFactor;

        gl_TessLevelInner[0] = tessellationFactor;
        gl_TessLevelInner[1] = tessellationFactor;
    }

	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outEntityId[gl_InvocationID] = inEntityId[gl_InvocationID];
    outGridUVs[gl_InvocationID] = inGridUVs[gl_InvocationID];
}
