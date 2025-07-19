#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in ivec4 inBoneIds;
layout(location = 5) in vec4 inBoneWeights;

layout(location = 6) in mat4 inTransform;
layout(location = 10) in uint inFirstMaterialIdx;
layout(location = 11) in uint inEntityId;

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out uint outMaterialIdx;
layout(location = 3) out mat3 outTBN;
layout(location = 6) out vec3 outWorldSpaceNormal;
layout(location = 7) out vec3 outWorldSpacePos;

struct BoneInfo
{
	mat4 transform;
};

const uint MAX_BONE_COUNT = 128;
layout(std430, set = 
#ifdef FORWARD
5
#else
1
#endif
, binding = 0) restrict buffer BoneInfoBufferLayout
{
    BoneInfo bonesInfo[MAX_BONE_COUNT];
};
 
out gl_PerVertex
{
    vec4 gl_Position;
};

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

const uint MAX_BONE_INFLUENCE = 4;

void main() 
{
	vec4 totalPosition = vec4(0.0f);
	vec3 totalNormal = vec3(0.0f);
	vec3 totalTangent = vec3(0.0f);
	for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
    {
        if (inBoneIds[i] == -1) 
            continue;

		vec4 localPosition = bonesInfo[inBoneIds[i]].transform * vec4(inPosition, 1.0f);
        totalPosition += localPosition * inBoneWeights[i];

        vec3 localNormal = transpose(inverse(mat3(bonesInfo[inBoneIds[i]].transform))) * inNormal;
		totalNormal += localNormal * inBoneWeights[i];

		vec3 localTangent = transpose(inverse(mat3(bonesInfo[inBoneIds[i]].transform))) * inTangent;
		totalTangent += localTangent * inBoneWeights[i];
    }

	vec4 viewPos = getViewMatrix() * inTransform * totalPosition;

    gl_Position = getProjectionMatrix() * viewPos;

    vec3 n = normalize(totalNormal);
	vec3 t = normalize(totalTangent);
	t = normalize(t - dot(t, n) * n);
	vec3 b = normalize(cross(t, n));
	outTBN = transpose(mat3(t, b, n));

	outViewPos = viewPos.xyz;
    outTexCoord = inTexCoord;
	outMaterialIdx = inFirstMaterialIdx;
	outWorldSpaceNormal = totalNormal;
	outWorldSpacePos =  (inTransform * vec4(inPosition, 1.0)).xyz;
} 
