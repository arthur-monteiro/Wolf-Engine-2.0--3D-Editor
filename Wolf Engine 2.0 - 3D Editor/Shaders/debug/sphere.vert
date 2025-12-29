layout(location = 0) in vec3 inPosition;

layout(location = 0) flat out uint outInstanceId;
 
void main() 
{
    outInstanceId = uint(gl_InstanceIndex);

    gl_Position = vec4(inPosition, 1.0f);
} 
