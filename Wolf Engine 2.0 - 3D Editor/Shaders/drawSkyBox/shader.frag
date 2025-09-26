layout (location = 0) in vec3 inTexCoords;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform samplerCube cubeMap;

void main() 
{   
    outColor = vec4(texture(cubeMap, inTexCoords).rgb, 1.0);
}