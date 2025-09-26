layout (location = 0) in vec3 inTexCoords;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() 
{
    vec2 uv = sampleSphericalMap(normalize(inTexCoords));
    vec3 color = texture(equirectangularMap, vec2(uv.x, -uv.y)).rgb;
    
    outColor = vec4(color, 1.0);
}