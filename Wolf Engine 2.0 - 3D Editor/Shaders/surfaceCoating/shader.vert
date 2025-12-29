#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in mat4 inTransform;
layout(location = 4) in uint inFirstMaterialIdx;
layout(location = 5) in uint inEntityId;

layout(location = 0) out uint outEntityId;
layout(location = 1) out vec2 outGridUVs;

const int GRID_SIZE = 31;

void main() 
{
	int patchID = gl_VertexIndex / 4;
    int vertInPatch = gl_VertexIndex % 4;

    int x = patchID % GRID_SIZE;
    int z = patchID / GRID_SIZE;

    // Local UVs for the 4 corners of the patch
    vec2 offsets[4] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), 
        vec2(0.0, 1.0), vec2(1.0, 1.0)
    );

    vec2 uv = (vec2(float(x), float(z)) + offsets[vertInPatch]) / float(GRID_SIZE);
    // world position (XZ plane, centered)
    vec3 localPos = vec3(uv.x - 0.5, 0.0, uv.y - 0.5);

    vec4 worldPos = inTransform * vec4(localPos, 1.0);
    worldPos.y = 30.0;

    gl_Position = worldPos;

	outEntityId = inEntityId;
    outGridUVs = uv;
} 
