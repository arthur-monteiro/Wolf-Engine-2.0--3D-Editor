layout (early_fragment_tests) in;

layout(binding = 0, set = 2, std140) uniform readonly UniformBuffer
{
	uint displayType;
} ub;

layout (location = 0) out vec4 outColor;

void main() 
{
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}