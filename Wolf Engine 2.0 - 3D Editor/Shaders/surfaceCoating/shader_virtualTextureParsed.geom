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
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) flat in uint inEntityId[];
layout(location = 1) in vec3 inNormal[];
layout(location = 2) in vec4 inMaterialWeights[];

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out uvec4 outMaterialIds;
layout(location = 4) out mat3 outTBN;
layout(location = 7) out vec3 outWorldSpaceNormal;
layout(location = 8) out vec3 outWorldSpacePos;
layout(location = 9) out uint outEntityId;
layout(location = 10) out vec4 outMaterialWeights;

#include "resources.glsl"

const float yThreshold = 0.5;

void main() 
{
    float minY = min(gl_in[0].gl_Position.y, min(gl_in[1].gl_Position.y, gl_in[2].gl_Position.y));
    float maxY = max(gl_in[0].gl_Position.y, max(gl_in[1].gl_Position.y, gl_in[2].gl_Position.y));
    float ySize = maxY - minY;
    if (ySize > yThreshold) 
    {
        return; 
    }

    for (int i = 0; i < 3; i++) 
    {
        vec4 viewPos = getViewMatrix() * vec4(gl_in[i].gl_Position.xyz, 1.0);
        gl_Position = getProjectionMatrix() * viewPos;

        outViewPos = viewPos.xyz;
        outColor = vec3(1, 1, 1);
        outTexCoord = vec2(0, 0); // unused because sampling should be triplanar
        outMaterialIds = ub.materialIndices;

        vec3 n = inNormal[i];
	    vec3 t = vec3(1, 0, 0);
	    vec3 b = normalize(cross(t, n));
	    outTBN = transpose(mat3(t, b, n));

        // TODO: pattern normal is evaluated per pixel and should apply something like this:
        // normal.xy *= vec2(ub.globalThickness);
        // normal = normalize(normal); 
        
        //outWorldSpaceNormal = TODO or unused?;
        outWorldSpacePos = gl_in[i].gl_Position.xyz;
        outEntityId = inEntityId[i];

        outMaterialWeights = inMaterialWeights[i];

        EmitVertex();
    }

    EndPrimitive();
}
