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
layout(quads, equal_spacing, ccw) in;

layout(location = 0) flat in uint inInstanceId[];
layout(location = 0) flat out uint outInstanceId;

const uint MAX_SPHERE_COUNT = 128;
layout(binding = 0, set = 2) uniform UniformBuffer
{
    vec4 worldPosAndRadius[MAX_SPHERE_COUNT];
    vec3 color;
    float padding;
} ub;

void main() 
{
    vec4 p1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
    vec4 p2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.x);  

    vec4 pos = mix(p1, p2, gl_TessCoord.y);
    pos.xyz = normalize(pos.xyz);
    pos.xyz *= ub.worldPosAndRadius[inInstanceId[0]].w;
    pos.xyz += ub.worldPosAndRadius[inInstanceId[0]].xyz;

    gl_Position = getProjectionMatrix() * getViewMatrix() * vec4(pos.xyz, 1.0f);

    outInstanceId = inInstanceId[0];
}
