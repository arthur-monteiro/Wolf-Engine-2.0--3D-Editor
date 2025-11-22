#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_nonuniform_qualifier : require

struct InstanceInfo
{
    uint firstMaterialIdx;
    uint vertexBufferBindlessOffset;
    uint indexBufferBindlessOffset;
};
const uint MAX_INSTANCES = 16384;
layout (binding = 0, set = @RAY_TRACING_DESCRIPTOR_SLOT) restrict buffer InstanceBufferLayout
{
    InstanceInfo instancesInfo[MAX_INSTANCES];
};

layout (binding = 1, set = @RAY_TRACING_DESCRIPTOR_SLOT) uniform accelerationStructureEXT topLevelAS;
layout (binding = 2, set = @RAY_TRACING_DESCRIPTOR_SLOT) readonly buffer BufferLayout { uint data[]; } buffers[];

struct Vertex
{
    vec3 pos;
	vec3 normal;
	vec3 tangent;
	vec2 texCoords;
	uint subMeshIdx;
};
uint vertexSize = 12;

#define traceRaySpecificPayload(_rayFlags, _cullMask, _sbtRecordOffset, _sbtRecordStride, _missIndex, _origin, _tmin, _direction, _tmax, _payloadLocation) \
    traceRayEXT(topLevelAS, _rayFlags, _cullMask, _sbtRecordOffset, _sbtRecordStride, _missIndex, _origin, _tmin, _direction, _tmax, _payloadLocation)

#define traceRay(_rayFlags, _cullMask, _sbtRecordOffset, _sbtRecordStride, _missIndex, _origin, _tmin, _direction, _tmax) \
    traceRaySpecificPayload(_rayFlags, _cullMask, _sbtRecordOffset, _sbtRecordStride, _missIndex, _origin, _tmin, _direction, _tmax, 0)

uint getFirstMaterialIdx(in uint instanceId)
{
    InstanceInfo instanceInfo = instancesInfo[instanceId];
    return instanceInfo.firstMaterialIdx;
}

ivec3 getIndices(in uint instanceId, in uint primitiveId)
{
    InstanceInfo instanceInfo = instancesInfo[instanceId];

    return ivec3(buffers[instanceInfo.indexBufferBindlessOffset].data[3 * primitiveId], buffers[instanceInfo.indexBufferBindlessOffset].data[3 * primitiveId + 1],
                 buffers[instanceInfo.indexBufferBindlessOffset].data[3 * primitiveId + 2]);
}

Vertex unpackVertex(in uint instanceId, in uint index)
{
    InstanceInfo instanceInfo = instancesInfo[instanceId];

    Vertex v;

    uint d0 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 0];
    uint d1 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 1];
    uint d2 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 2];
    uint d3 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 3];
    uint d4 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 4];
    uint d5 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 5];
    uint d6 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 6];
    uint d7 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 7];
    uint d8 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 8];
    uint d9 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 9];
    uint d10 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 10];
    uint d11 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 11];

    v.pos = vec3(uintBitsToFloat(d0), uintBitsToFloat(d1), uintBitsToFloat(d2));
    v.normal = vec3(uintBitsToFloat(d3), uintBitsToFloat(d4), uintBitsToFloat(d5));
    v.tangent = vec3(uintBitsToFloat(d6), uintBitsToFloat(d7), uintBitsToFloat(d8));
    v.texCoords = vec2(uintBitsToFloat(d9), uintBitsToFloat(d10));
    v.subMeshIdx = d11;

    return v;
}

Vertex computeVertex(in uint instanceId, in uint primitiveId, in vec2 attribs)
{
    ivec3 indices = getIndices(instanceId, primitiveId);

    Vertex v0 = unpackVertex(instanceId, indices.x);
    Vertex v1 = unpackVertex(instanceId, indices.y);
    Vertex v2 = unpackVertex(instanceId, indices.z);

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    
    Vertex v;
    v.pos = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
    v.normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
    v.tangent = v0.tangent * barycentrics.x + v1.tangent * barycentrics.y + v2.tangent * barycentrics.z;
    v.texCoords = v0.texCoords * barycentrics.x + v1.texCoords * barycentrics.y + v2.texCoords * barycentrics.z;
    v.subMeshIdx = v0.subMeshIdx;

    return v;
}