#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

const uint MAX_RECTANGLES_COUNT = 128;
layout(binding = 0, set = 2) uniform UniformBuffer
{
    mat4 transform[MAX_RECTANGLES_COUNT];
} ub;
 
out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
	vec4 viewPos = getViewMatrix() * ub.transform[gl_InstanceIndex] * vec4(inPosition, 1.0);
    gl_Position = getProjectionMatrix() * viewPos;    
} 
