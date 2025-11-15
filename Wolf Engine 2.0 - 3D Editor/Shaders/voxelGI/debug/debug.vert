#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in uint inMaterialID;

layout(location = 5) in vec3 inWorldPos;

layout(location = 0) out uint outVoxelIdx;
layout(location = 1) out vec3 outDirection;
 
out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
    vec4 localPos = vec4(inPosition * 0.005, 1.0);

	vec4 worldPos = localPos + vec4(inWorldPos, 0.0);
	vec4 viewPos = getViewMatrix() * worldPos;

    gl_Position = getProjectionMatrix() * viewPos;
    outVoxelIdx = gl_InstanceIndex;
    outDirection = normalize(inPosition);
} 
