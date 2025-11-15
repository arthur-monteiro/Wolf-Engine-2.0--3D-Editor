#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout (early_fragment_tests) in;

layout (location = 0) flat in uint inVoxelIdx;
layout (location = 1) in vec3 inDirection;

layout (location = 0) out vec4 outColor;

layout(std430, binding = 0, set = 2) buffer VoxelGridLayout
{
    vec3 irradiances[];
} voxelGrid;

vec3 computeIrradianceForDirection(in uint voxelIdx, in vec3 direction)
{
    vec3 absDirection = abs(direction);
    
    float w_x = absDirection.x;
    float w_y = absDirection.y;
    float w_z = absDirection.z;
    
    vec3 color_x = (direction.x >= 0.0) ? voxelGrid.irradiances[voxelIdx * 6 + 0] : voxelGrid.irradiances[voxelIdx * 6 + 1]; // +X or -X
    vec3 color_y = (direction.y >= 0.0) ? voxelGrid.irradiances[voxelIdx * 6 + 2] : voxelGrid.irradiances[voxelIdx * 6 + 3]; // +Y or -Y
    vec3 color_z = (direction.z >= 0.0) ? voxelGrid.irradiances[voxelIdx * 6 + 4] : voxelGrid.irradiances[voxelIdx * 6 + 5]; // +Z or -Z
    
    vec3 blendedColor = color_x * w_x + color_y * w_y + color_z * w_z;
    
    float weightSum = w_x + w_y + w_z;
    if (weightSum > 0.000001) 
    {
        return blendedColor / weightSum;
    }

    return vec3(0.0);
}

void main() 
{
    outColor = vec4(computeIrradianceForDirection(inVoxelIdx, inDirection), 1.0);
}