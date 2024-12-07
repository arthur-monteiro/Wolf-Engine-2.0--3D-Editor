#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in uint inSubMeshIdx;

layout(location = 5) in mat4 inTransform;
layout(location = 9) in uint inFirstMaterialIdx;

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out uint outMaterialIdx;
layout(location = 3) out mat3 outTBN;
layout(location = 6) out vec3 outWorldSpaceNormal;
layout(location = 7) out vec3 outWorldSpacePos;
 
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
	vec4 viewPos = getViewMatrix() * inTransform * vec4(inPosition, 1.0);

    gl_Position = getProjectionMatrix() * viewPos;

	mat3 usedModelMatrix = transpose(inverse(mat3(inTransform)));
    vec3 n = normalize(usedModelMatrix * inNormal);
	vec3 t = normalize(usedModelMatrix * inTangent);
	t = normalize(t - dot(t, n) * n);
	vec3 b = normalize(cross(t, n));
	outTBN = transpose(mat3(t, b, n));

	outViewPos = viewPos.xyz;
    outTexCoord = inTexCoord;
	outMaterialIdx = inFirstMaterialIdx + inSubMeshIdx;
	outWorldSpaceNormal = normalize(inNormal);
	outWorldSpacePos =  (inTransform * vec4(inPosition, 1.0)).xyz;
} 
