#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in ivec4 inBoneIds;
layout(location = 5) in vec4 inBoneWeights;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out uint outMaterialID;
 
out gl_PerVertex
{
    vec4 gl_Position;
};

struct BoneInfo
{
	mat4 transform;
};

const uint MAX_BONE_COUNT = 128;
layout(std430, set = 2, binding = 0) restrict buffer BoneInfoBufferLayout
{
    BoneInfo bonesInfo[MAX_BONE_COUNT];
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

	vec4 viewPos = getViewMatrix() * totalPosition;

    gl_Position = getProjectionMatrix() * viewPos;

    outTexCoord = inTexCoord;
	outMaterialID = 0;
} 
