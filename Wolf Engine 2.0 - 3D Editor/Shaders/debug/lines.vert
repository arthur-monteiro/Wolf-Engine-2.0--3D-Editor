#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, set = 2) uniform UniformBuffer
{
    mat4 transform;
} ub;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
 
out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
	vec4 viewPos = getViewMatrix() * ub.transform * vec4(inPosition, 1.0);
    gl_Position = getProjectionMatrix() * viewPos;    
} 
