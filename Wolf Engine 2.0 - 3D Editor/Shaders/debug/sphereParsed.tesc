#version 460

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
layout (vertices = 4) out;

layout(location = 0) flat in uint inInstanceId[];
layout(location = 0) flat out uint outInstanceId[];

void main() 
{
    float tl = 32.0;
    if (gl_InvocationID == 0)
    {
        gl_TessLevelInner[0] = tl;
        gl_TessLevelInner[1] = tl;
        
        gl_TessLevelOuter[0] = tl;
        gl_TessLevelOuter[1] = tl;
        gl_TessLevelOuter[2] = tl;
        gl_TessLevelOuter[3] = tl;
    }

	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outInstanceId[gl_InvocationID] = inInstanceId[gl_InvocationID];
}
