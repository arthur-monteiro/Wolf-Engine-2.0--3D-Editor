layout(std430, binding = 0, set = @VOXEL_GI_DESCRIPTOR_SLOT) buffer VoxelGridLayout
{
    vec3 values[];
} voxelGrid;

layout(std430, binding = 1, set = @VOXEL_GI_DESCRIPTOR_SLOT) buffer VoxelRequestLayout
{
    uint values[];
} voxelRequest;

layout(std430, binding = 2, set = @VOXEL_GI_DESCRIPTOR_SLOT) buffer VoxelRequestCopyLayout
{
    uint values[];
} voxelRequestCopy;

const vec3 FIRST_PROBE_POS = vec3(-16.0f, -5.0f, -16.0f);
const float SPACE_BETWEEN_PROBES = 1.0f;
const uint GRID_SIZE = 32;

const vec3 DIRECTIONS[] = {
    vec3(1, 0, 0),
    vec3(-1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, -1, 0),
    vec3(0, 0, 1),
    vec3(0, 0, -1)
};

void requestFace(in uint requestValueIdx, in uint firstRequestBitIdx, in uint bitOffset, in uint voxelIdx, in vec3 worldPos)
{
    uint faceIdx = bitOffset;

    if (firstRequestBitIdx + bitOffset >= 32)
    {
        requestValueIdx++;
        bitOffset = (firstRequestBitIdx + bitOffset) - 32;
        firstRequestBitIdx = 0;
    }
    uint mask = (1u << (firstRequestBitIdx + bitOffset));
    uint requestBefore = atomicOr(voxelRequest.values[requestValueIdx], mask);

    if ((requestBefore & mask) == 0u) // first to request
    {
        vec3 previousPos = voxelGrid.values[voxelIdx * 12 + 6 + faceIdx];
        float lerpCoeff = previousPos == vec3(0.0) ? 1.0 : 0.01;
        voxelGrid.values[voxelIdx * 12 + 6 + faceIdx] = mix(previousPos, worldPos, lerpCoeff);
    }
}

vec3 computeAndRequestIrradianceForDirection(in uint voxelIdx, in vec3 direction, in vec3 worldPos)
{
    vec3 absDirection = abs(direction);
        
    float w_x = absDirection.x;
    float w_y = absDirection.y;
    float w_z = absDirection.z;

    const uint requestValueIdx = (6 * voxelIdx) / 32;
    const uint firstRequestBitIdx = (6 * voxelIdx) % 32;
    
    vec3 color_x;
    if (direction.x >= 0.0) // +X or -X
    {
        color_x = voxelGrid.values[voxelIdx * 12 + 0];
        requestFace(requestValueIdx, firstRequestBitIdx, 0, voxelIdx, worldPos);
    }
    else 
    {
        color_x = voxelGrid.values[voxelIdx * 12 + 1];
        requestFace(requestValueIdx, firstRequestBitIdx, 1, voxelIdx, worldPos);
    }

    vec3 color_y;
    if (direction.y >= 0.0) // +Y or -Y
    {
        color_y = voxelGrid.values[voxelIdx * 12 + 2];
        requestFace(requestValueIdx, firstRequestBitIdx, 2, voxelIdx, worldPos);
    }
    else
    {
        color_y = voxelGrid.values[voxelIdx * 12 + 3];
        requestFace(requestValueIdx, firstRequestBitIdx, 3, voxelIdx, worldPos);
    }

    vec3 color_z;
    if (direction.z >= 0.0) // +Z or -Z
    {
        color_z = voxelGrid.values[voxelIdx * 12 + 4];
        requestFace(requestValueIdx, firstRequestBitIdx, 4, voxelIdx, worldPos);
    }
    else
    {
        color_z = voxelGrid.values[voxelIdx * 12 + 5];
        requestFace(requestValueIdx, firstRequestBitIdx, 5, voxelIdx, worldPos);
    }
    
    vec3 blendedColor = color_x * w_x + color_y * w_y + color_z * w_z;
    float weightSum = w_x + w_y + w_z;
    if (weightSum > 0.000001) 
    {
        return blendedColor / weightSum;
    }
    
    return vec3(0.0);
}

#define CLAMP_TO_GRID(val) clamp((val), 0, int(GRID_SIZE) - 1)
#define GET_VOXEL_IDX(ix, iy, iz) (uint(ix) + uint(iy) * GRID_SIZE + uint(iz) * GRID_SIZE * GRID_SIZE)

void computeTrilinearVoxelIndices(in vec3 probeCoordsFloat, out uint indices[8])
{
    vec3 shiftedCoords = probeCoordsFloat - 0.5;
    ivec3 probeCoordsInt = ivec3(floor(shiftedCoords));

    int x0 = probeCoordsInt.x;
    int y0 = probeCoordsInt.y;
    int z0 = probeCoordsInt.z;

    int x1 = x0 + 1;
    int y1 = y0 + 1;
    int z1 = z0 + 1;

    // P000: (x0, y0, z0)
    ivec3 c000 = ivec3(CLAMP_TO_GRID(x0), CLAMP_TO_GRID(y0), CLAMP_TO_GRID(z0));
    indices[0] = GET_VOXEL_IDX(c000.x, c000.y, c000.z);
    
    // P100: (x1, y0, z0)
    ivec3 c100 = ivec3(CLAMP_TO_GRID(x1), CLAMP_TO_GRID(y0), CLAMP_TO_GRID(z0));
    indices[1] = GET_VOXEL_IDX(c100.x, c100.y, c100.z);
    
    // P010: (x0, y1, z0)
    ivec3 c010 = ivec3(CLAMP_TO_GRID(x0), CLAMP_TO_GRID(y1), CLAMP_TO_GRID(z0));
    indices[2] = GET_VOXEL_IDX(c010.x, c010.y, c010.z);

    // P110: (x1, y1, z0)
    ivec3 c110 = ivec3(CLAMP_TO_GRID(x1), CLAMP_TO_GRID(y1), CLAMP_TO_GRID(z0));
    indices[3] = GET_VOXEL_IDX(c110.x, c110.y, c110.z);

    // P001: (x0, y0, z1)
    ivec3 c001 = ivec3(CLAMP_TO_GRID(x0), CLAMP_TO_GRID(y0), CLAMP_TO_GRID(z1));
    indices[4] = GET_VOXEL_IDX(c001.x, c001.y, c001.z);

    // P101: (x1, y0, z1)
    ivec3 c101 = ivec3(CLAMP_TO_GRID(x1), CLAMP_TO_GRID(y0), CLAMP_TO_GRID(z1));
    indices[5] = GET_VOXEL_IDX(c101.x, c101.y, c101.z);

    // P011: (x0, y1, z1)
    ivec3 c011 = ivec3(CLAMP_TO_GRID(x0), CLAMP_TO_GRID(y1), CLAMP_TO_GRID(z1));
    indices[6] = GET_VOXEL_IDX(c011.x, c011.y, c011.z);

    // P111: (x1, y1, z1)
    ivec3 c111 = ivec3(CLAMP_TO_GRID(x1), CLAMP_TO_GRID(y1), CLAMP_TO_GRID(z1));
    indices[7] = GET_VOXEL_IDX(c111.x, c111.y, c111.z);
}

bool isFaceRequested(in uint requestValueIdx, in uint firstRequestBitIdx, in uint bitOffset, in uint voxelIdx)
{
    if (firstRequestBitIdx + bitOffset >= 32)
    {
        requestValueIdx++;
        bitOffset = (firstRequestBitIdx + bitOffset) - 32;
        firstRequestBitIdx = 0;
    }
    uint mask = (1u << (firstRequestBitIdx + bitOffset));
    return (voxelRequestCopy.values[requestValueIdx] & mask) != 0u;
}

vec3 computeTrilinearIrradianceForFaceNoRequest(in uint indices[8], in uint faceIdx, in vec3 worldPos)
{
    vec3 cummulatedIrradiance = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0; i < 8; ++i)
    {
        uint voxelIdx = indices[i];

        const uint requestValueIdx = (6 * voxelIdx) / 32;
        const uint firstRequestBitIdx = (6 * voxelIdx) % 32;

        if (isFaceRequested(requestValueIdx, firstRequestBitIdx, faceIdx, voxelIdx))
        {
            uint z = voxelIdx / (GRID_SIZE * GRID_SIZE); 
            uint y = (voxelIdx / GRID_SIZE) % GRID_SIZE; 
            uint x = voxelIdx % GRID_SIZE;
            vec3 probeCenter = FIRST_PROBE_POS + vec3(x, y, z) * SPACE_BETWEEN_PROBES + vec3(SPACE_BETWEEN_PROBES) * 0.5;
            vec3 probePos = voxelGrid.values[voxelIdx * 12 + 6 + faceIdx];

            float weight = clamp(SPACE_BETWEEN_PROBES - distance(probeCenter, worldPos), 0.0, 1.0);
            cummulatedIrradiance += voxelGrid.values[voxelIdx * 12 + faceIdx] * weight;
            totalWeight += weight;
        }
    }
    return cummulatedIrradiance / max(totalWeight, 0.001);
}

vec3 computeIrradiance(in vec3 worldPos, in vec3 direction, in bool trilinear)
{
    vec3 usedWorldPos = worldPos + normalize(direction) * SPACE_BETWEEN_PROBES * 0.5;
    vec3 probeCoordsFloat = (usedWorldPos - FIRST_PROBE_POS) / SPACE_BETWEEN_PROBES;

    if (trilinear)
    {
        ivec3 noMixProbeCoordsInt = ivec3(floor(probeCoordsFloat));
        ivec3 c = ivec3(CLAMP_TO_GRID(noMixProbeCoordsInt.x), CLAMP_TO_GRID(noMixProbeCoordsInt.y), CLAMP_TO_GRID(noMixProbeCoordsInt.z));
        uint voxelIdx = GET_VOXEL_IDX(c.x, c.y, c.z);

        computeAndRequestIrradianceForDirection(voxelIdx, direction, usedWorldPos); // just to make the request

        uint indices[8];
        computeTrilinearVoxelIndices(probeCoordsFloat, indices);

        vec3 colorX;
        if (direction.x >= 0.0) // +X or -X
        {
            colorX = computeTrilinearIrradianceForFaceNoRequest(indices, 0, usedWorldPos);
        }
        else 
        {
            colorX = computeTrilinearIrradianceForFaceNoRequest(indices, 1, usedWorldPos);
        }

        vec3 colorY;
        if (direction.y >= 0.0) // +Y or -Y
        {
            colorY = computeTrilinearIrradianceForFaceNoRequest(indices, 2, usedWorldPos);
        }
        else
        {
            colorY = computeTrilinearIrradianceForFaceNoRequest(indices, 3, usedWorldPos);
        }

        vec3 colorZ;
        if (direction.z >= 0.0) // +Z or -Z
        {
            colorZ = computeTrilinearIrradianceForFaceNoRequest(indices, 4, usedWorldPos);
        }
        else
        {
            colorZ = computeTrilinearIrradianceForFaceNoRequest(indices, 5, usedWorldPos);
        }

        vec3 absDirection = abs(direction);
        
        float wX = absDirection.x;
        float wY = absDirection.y;
        float wZ = absDirection.z;

        vec3 blendedColor = colorX * wX + colorY * wY + colorZ * wZ;
        float weightSum = wX + wY + wZ;
        if (weightSum > 0.000001) 
        {
            return blendedColor / weightSum;
        }
        return vec3(0.0);
    }
    else
    {
        ivec3 probeCoordsInt = ivec3(floor(probeCoordsFloat));

        int x = clamp(probeCoordsInt.x, 0, int(GRID_SIZE) - 1);
        int y = clamp(probeCoordsInt.y, 0, int(GRID_SIZE) - 1);
        int z = clamp(probeCoordsInt.z, 0, int(GRID_SIZE) - 1);

        uint voxelIdx = uint(x) + uint(y) * GRID_SIZE + uint(z) * GRID_SIZE * GRID_SIZE;

        return computeAndRequestIrradianceForDirection(voxelIdx, direction, usedWorldPos);
    }
}