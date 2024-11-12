layout (early_fragment_tests) in;

layout (location = 0) in vec2 inTexCoords;
layout (location = 1) flat in uint inMaterialID;

layout (location = 0) out vec4 outColor;

void main() 
{
    MaterialInfo materialInfo = fetchMaterial(inTexCoords, inMaterialID, mat3(1.0), vec3(1.0));
    outColor = vec4(materialInfo.albedo.rgb, 1.0);
}