layout (vertices = 4) out;

layout(location = 0) flat in uint inEntityId[];
layout(location = 1) in vec2 inGridUVs[];

layout(location = 0) flat out uint outEntityId[];
layout(location = 1) out vec2 outGridUVs[];

void main() 
{
    float tl = 16.0;
    if (gl_InvocationID == 0)
    {
        gl_TessLevelInner[0] = tl;
        gl_TessLevelInner[1] = tl;
        
        gl_TessLevelOuter[0] = tl;
        gl_TessLevelOuter[1] = tl;
        gl_TessLevelOuter[2] = tl;
        gl_TessLevelOuter[3] = tl;
    }

	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outEntityId[gl_InvocationID] = inEntityId[gl_InvocationID];
    outGridUVs[gl_InvocationID] = inGridUVs[gl_InvocationID];
}