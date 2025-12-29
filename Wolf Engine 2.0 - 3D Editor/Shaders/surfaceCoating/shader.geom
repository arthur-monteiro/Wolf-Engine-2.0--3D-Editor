layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) flat in uint inEntityId[];
layout(location = 1) in vec3 inNormal[];

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out uint outMaterialIdx;
layout(location = 4) out mat3 outTBN;
layout(location = 7) out vec3 outWorldSpaceNormal;
layout(location = 8) out vec3 outWorldSpacePos;
layout(location = 9) out uint outEntityId;

#include "resources.glsl"

const float yThreshold = 0.5;

void main() 
{
    float minY = min(gl_in[0].gl_Position.y, min(gl_in[1].gl_Position.y, gl_in[2].gl_Position.y));
    float maxY = max(gl_in[0].gl_Position.y, max(gl_in[1].gl_Position.y, gl_in[2].gl_Position.y));
    float ySize = maxY - minY;
    if (ySize > yThreshold) 
    {
        return; 
    }

    for (int i = 0; i < 3; i++) 
    {
        vec4 viewPos = getViewMatrix() * vec4(gl_in[i].gl_Position.xyz, 1.0);
        gl_Position = getProjectionMatrix() * viewPos;

        outViewPos = viewPos.xyz;
        outColor = vec3(1, 1, 1);
        outTexCoord = vec2(0, 0); // unused because sampling should be triplanar
        outMaterialIdx = ub.materialIdx;

        vec3 n = inNormal[i];
	    vec3 t = vec3(1, 0, 0);
	    vec3 b = normalize(cross(t, n));
	    outTBN = transpose(mat3(t, b, n));
        
        //outWorldSpaceNormal = TODO or unused?;
        outWorldSpacePos = gl_in[i].gl_Position.xyz;
        outEntityId = inEntityId[i];

        EmitVertex();
    }

    EndPrimitive();
}