layout (vertices = 4) out;

layout(location = 0) flat in uint inInstanceId[];
layout(location = 0) flat out uint outInstanceId[];

void main() 
{
    float tl = 32.0;
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
    outInstanceId[gl_InvocationID] = inInstanceId[gl_InvocationID];
}