layout(quads, equal_spacing, ccw) in;

layout(location = 0) flat in uint inInstanceId[];

const uint MAX_SPHERE_COUNT = 16;
layout(binding = 0, set = 2) uniform UniformBuffer
{
    vec4 worldPosAndRadius[MAX_SPHERE_COUNT];
} ub;

void main() 
{
    vec4 p1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
    vec4 p2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.x);  

    vec4 pos = mix(p1, p2, gl_TessCoord.y);
    pos.xyz = normalize(pos.xyz);
    pos.xyz *= ub.worldPosAndRadius[inInstanceId[0]].w;
    pos.xyz += ub.worldPosAndRadius[inInstanceId[0]].xyz;

    gl_Position = getProjectionMatrix() * getViewMatrix() * vec4(pos.xyz, 1.0f);
}
