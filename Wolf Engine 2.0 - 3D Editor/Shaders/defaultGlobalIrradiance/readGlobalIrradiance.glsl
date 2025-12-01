layout(binding = 0, set = @VOXEL_GI_DESCRIPTOR_SLOT, std140) uniform readonly UniformBuffer
{
	float ambientIntensity;
} ubGlobalIrradiance;

vec3 computeIrradiance(in vec3 worldPos, in vec3 direction, in bool trilinear)
{
    return vec3(ubGlobalIrradiance.ambientIntensity);
}