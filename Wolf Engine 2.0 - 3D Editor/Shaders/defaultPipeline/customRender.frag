layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) flat in uint inMaterialID;
layout (location = 3) in mat3 inTBN;
layout (location = 6) in vec3 inWorldSpaceNormal;
layout (location = 7) in vec3 inWorldSpacePos;
layout (location = 8) flat in uint inEntityId;

#ifdef ALBEDO
layout (location = ALBEDO_LOCATION) out vec4 outAlbedo;
#endif

#ifdef NORMAL
layout (location = NORMAL_LOCATION) out vec4 outNormal;
#endif

#ifdef VERTEX_NORMAL
layout (location = VERTEX_NORMAL_LOCATION) out vec4 outVertexNormal;
#endif

void main() 
{
    MaterialInfo materialInfo = fetchMaterial(inTexCoords, inMaterialID, inTBN, computeWorldPosFromViewPos(inViewPos));

#ifdef ALBEDO
    outAlbedo = vec4(materialInfo.albedo.rgb, 1.0);
#endif

#ifdef NORMAL
    outNormal = vec4(materialInfo.normal.xyz, 1.0);
#endif

#ifdef VERTEX_NORMAL
    outVertexNormal = vec4(inTBN[0].z, inTBN[1].z, inTBN[2].z, 1.0);
#endif
}