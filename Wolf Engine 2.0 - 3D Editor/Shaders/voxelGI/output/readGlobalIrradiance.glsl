layout(std430, binding = 0, set = @VOXEL_GI_DESCRIPTOR_SLOT) buffer VoxelGridLayout
{
    vec3 irradiances[];
} voxelGrid;

layout(std430, binding = 1, set = @VOXEL_GI_DESCRIPTOR_SLOT) buffer VoxelRequestLayout
{
    uint values[];
} voxelRequest;

const vec3 FIRST_PROBE_POS = vec3(-10.0f, -5.0f, -10.0f);
const float SPACE_BETWEEN_PROBES = 1.0f;
const uint GRID_SIZE = 32;

void setRequestBit(in uint requestValueIdx, in uint firstRequestBitIdx, in uint bitOffset)
{
    if (firstRequestBitIdx + bitOffset >= 32)
    {
        requestValueIdx++;
        bitOffset = (firstRequestBitIdx + bitOffset) - 32;
        firstRequestBitIdx = 0;
    }
    atomicOr(voxelRequest.values[requestValueIdx], 1 << (firstRequestBitIdx + bitOffset));
}

vec3 computeIrradianceForDirection(in uint voxelIdx, in vec3 direction)
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
        color_x = voxelGrid.irradiances[voxelIdx * 6 + 0];
        atomicOr(voxelRequest.values[requestValueIdx], 1 << firstRequestBitIdx);
    }
    else 
    {
        color_x = voxelGrid.irradiances[voxelIdx * 6 + 1];
        setRequestBit(requestValueIdx, firstRequestBitIdx, 1);
    }

    vec3 color_y;
    if (direction.y >= 0.0) // +Y or -Y
    {
        color_y = voxelGrid.irradiances[voxelIdx * 6 + 2];
        setRequestBit(requestValueIdx, firstRequestBitIdx, 2);
    }
    else
    {
        color_y = voxelGrid.irradiances[voxelIdx * 6 + 3];
        setRequestBit(requestValueIdx, firstRequestBitIdx, 3);
    }

    vec3 color_z;
    if (direction.z >= 0.0) // +Z or -Z
    {
        color_z = voxelGrid.irradiances[voxelIdx * 6 + 4];
        setRequestBit(requestValueIdx, firstRequestBitIdx, 4);
    }
    else
    {
        color_z = voxelGrid.irradiances[voxelIdx * 6 + 5];
        setRequestBit(requestValueIdx, firstRequestBitIdx, 5);
    }
    
    vec3 blendedColor = color_x * w_x + color_y * w_y + color_z * w_z;
    float weightSum = w_x + w_y + w_z;
    if (weightSum > 0.000001) 
    {
        return blendedColor / weightSum;
    }
    
    return vec3(0.0);
}

vec3 computeIrradiance(in vec3 worldPos, in vec3 direction)
{
    vec3 usedWorldPos = worldPos + normalize(direction) * SPACE_BETWEEN_PROBES * 0.5;
    vec3 probeCoords = (usedWorldPos - FIRST_PROBE_POS) / SPACE_BETWEEN_PROBES + vec3(SPACE_BETWEEN_PROBES * 0.5);
    uint voxelIdx = uint(probeCoords.x) + uint(probeCoords.y) * GRID_SIZE + uint(probeCoords.z) * GRID_SIZE * GRID_SIZE;

    if (voxelIdx > GRID_SIZE * GRID_SIZE * GRID_SIZE)
        return vec3(0.0);

    return computeIrradianceForDirection(voxelIdx, direction);
}