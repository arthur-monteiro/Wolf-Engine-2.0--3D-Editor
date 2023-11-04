#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, set = 1) uniform UniformBufferMVP
{
    mat4 model;
	mat4 view;
	mat4 projection;
} ubMVP;

layout(binding = 0, set = 3) uniform UniformBufferBuildingMesh
{
    vec2 scale;
	vec2 offset;
} ubBuildingMesh;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in uint inMaterialID;

layout(location = 5) in vec3 inPositionOffset;
layout(location = 6) in vec3 inNewNormal;

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out uint outMaterialID;
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
	vec3 inputPosition = inPosition;
	inputPosition.xy += ubBuildingMesh.offset;
	inputPosition.xy *= ubBuildingMesh.scale;
	vec3 meshDefaultNormal = vec3(0, 0, 1);
	vec3 newNormal = inNewNormal;

	float cosTheta = newNormal.z;
    mat3 rotMatrix = mat3(1.0f);

    if(abs(cosTheta) < 0.9f)
    {
        float sinTheta = sqrt(newNormal.x * newNormal.x + newNormal.y * newNormal.y);

        vec3 R = normalize(cross(newNormal, meshDefaultNormal));
        float oneMinCosTheta = 1 - cosTheta;

        rotMatrix = mat3(cosTheta + (R.x * R.x) * oneMinCosTheta,        R.x * R.y * oneMinCosTheta - R.z * sinTheta,    R.x * R.z * oneMinCosTheta + R.y * sinTheta,
                         R.y * R.x * oneMinCosTheta + R.z * sinTheta,    cosTheta + (R.y * R.y) * oneMinCosTheta,        R.y * R.z * oneMinCosTheta - R.x * sinTheta,
                         R.z * R.x * oneMinCosTheta - R.y * sinTheta,    R.z * R.y * oneMinCosTheta + R.x * sinTheta,    cosTheta + (R.z * R.z) * oneMinCosTheta);

		inputPosition = rotMatrix * inputPosition;
    }
	else if(cosTheta < 0.0f)
	{
		rotMatrix = mat3(-1, 0, 0,
		                  0, 1, 0,
						  0, 0, -1);

		inputPosition = rotMatrix * inputPosition;
	}

	vec4 viewPos = ubMVP.view * ubMVP.model * vec4(inputPosition + inPositionOffset, 1.0);

    gl_Position = ubMVP.projection * viewPos;

	mat3 usedModelMatrix = transpose(inverse(mat3(ubMVP.view * ubMVP.model)));
    vec3 n = normalize(usedModelMatrix * inNormal);
	vec3 t = normalize(usedModelMatrix * inTangent);
	t = normalize(t - dot(t, n) * n);
	vec3 b = normalize(cross(t, n));
	outTBN = inverse(mat3(t, b, n));

	outViewPos = viewPos.xyz;
    outTexCoord = inTexCoord;
	outMaterialID = inMaterialID;
	outWorldSpaceNormal = normalize(inNormal);
	outWorldSpacePos =  (ubMVP.model * vec4(inPosition, 1.0)).xyz;
} 
