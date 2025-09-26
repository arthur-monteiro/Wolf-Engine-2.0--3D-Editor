#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_draw_instanced : enable

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outTexCoords;
 
out gl_PerVertex
{
    vec4 gl_Position;
};

layout(binding = 1) uniform UniformBuffer 
{
    mat4 viewProj;
} ub;


void main() 
{	
	vec4 pos = ub.viewProj * vec4(inPosition, 1.0);
    gl_Position = vec4(pos.xy, pos.w, pos.w);

	outTexCoords = inPosition * 2.0;
} 
