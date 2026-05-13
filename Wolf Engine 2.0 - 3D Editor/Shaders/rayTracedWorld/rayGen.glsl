#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_nonuniform_qualifier : require

struct InstanceInfo
{
    uint materialIdx;
    uint vertexBufferBindlessOffset;
    uint vertexBufferOffset;
    uint indexBufferBindlessOffset;
    uint indexBufferOffset;
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

    vec3 triangleNormal;
};
uint vertexSize = 11;

#define traceRaySpecificPayload(_rayFlags, _cullMask, _sbtRecordOffset, _sbtRecordStride, _missIndex, _origin, _tmin, _direction, _tmax, _payloadLocation) \
    traceRayEXT(topLevelAS, _rayFlags, _cullMask, _sbtRecordOffset, _sbtRecordStride, _missIndex, _origin, _tmin, _direction, _tmax, _payloadLocation)

#define traceRay(_rayFlags, _cullMask, _sbtRecordOffset, _sbtRecordStride, _missIndex, _origin, _tmin, _direction, _tmax) \
    traceRaySpecificPayload(_rayFlags, _cullMask, _sbtRecordOffset, _sbtRecordStride, _missIndex, _origin, _tmin, _direction, _tmax, 0)

uint getMaterialIdx(in uint instanceId)
{
    InstanceInfo instanceInfo = instancesInfo[instanceId];
    return instanceInfo.materialIdx;
}

ivec3 getIndices(in uint instanceId, in uint primitiveId)
{
    InstanceInfo instanceInfo = instancesInfo[instanceId];

    return ivec3(buffers[instanceInfo.indexBufferBindlessOffset].data[3 * primitiveId + instanceInfo.indexBufferOffset], buffers[instanceInfo.indexBufferBindlessOffset].data[3 * primitiveId + 1 + instanceInfo.indexBufferOffset],
                 buffers[instanceInfo.indexBufferBindlessOffset].data[3 * primitiveId + 2 + instanceInfo.indexBufferOffset]);
}

Vertex unpackVertex(in uint instanceId, in uint index)
{
    InstanceInfo instanceInfo = instancesInfo[instanceId];

    Vertex v;

    uint d0 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 0 + instanceInfo.vertexBufferOffset];
    uint d1 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 1 + instanceInfo.vertexBufferOffset];
    uint d2 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 2 + instanceInfo.vertexBufferOffset];
    uint d3 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 3 + instanceInfo.vertexBufferOffset];
    uint d4 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 4 + instanceInfo.vertexBufferOffset];
    uint d5 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 5 + instanceInfo.vertexBufferOffset];
    uint d6 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 6 + instanceInfo.vertexBufferOffset];
    uint d7 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 7 + instanceInfo.vertexBufferOffset];
    uint d8 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 8 + instanceInfo.vertexBufferOffset];
    uint d9 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 9 + instanceInfo.vertexBufferOffset];
    uint d10 = buffers[instanceInfo.vertexBufferBindlessOffset].data[vertexSize * index + 10 + instanceInfo.vertexBufferOffset];

    v.pos = vec3(uintBitsToFloat(d0), uintBitsToFloat(d1), uintBitsToFloat(d2));
    v.normal = vec3(uintBitsToFloat(d3), uintBitsToFloat(d4), uintBitsToFloat(d5));
    v.tangent = vec3(uintBitsToFloat(d6), uintBitsToFloat(d7), uintBitsToFloat(d8));
    v.texCoords = vec2(uintBitsToFloat(d9), uintBitsToFloat(d10));

    return v;
}

vec3 computeTriangleNormal(vec3 p1, vec3 p2, vec3 p3) 
{
    // Edge vectors
    vec3 A = p2 - p1;
    vec3 B = p3 - p1;

    vec3 normal = cross(A, B);
    return normalize(normal);
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
    v.triangleNormal = computeTriangleNormal(v0.pos, v1.pos, v2.pos);
    v.tangent = v0.tangent * barycentrics.x + v1.tangent * barycentrics.y + v2.tangent * barycentrics.z;
    v.texCoords = v0.texCoords * barycentrics.x + v1.texCoords * barycentrics.y + v2.texCoords * barycentrics.z;

    return v;
}