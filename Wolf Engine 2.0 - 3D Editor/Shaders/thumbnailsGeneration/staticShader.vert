#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec2 inTexCoord;
layout(location = 5) in uint inMaterialID;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out uint outMaterialID;
 
layout(binding = 0, set = 2) uniform UniformBuffer
{
	uint firstMaterialIdx;
} ub;

out gl_PerVertex
{
    vec4 gl_Position;
};

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() 
{
	vec4 viewPos = getViewMatrix() * vec4(inPosition, 1.0);

    gl_Position = getProjectionMatrix() * viewPos;

    outTexCoord = inTexCoord;
	outMaterialID = inMaterialID + ub.firstMaterialIdx;
} 
