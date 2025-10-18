float srgbToLinear(float c) 
{
    if (c <= 0.04045) 
    {
        return c / 12.92;
    }
    else 
    {
        return pow((c + 0.055) / 1.055, 2.4);
    }
}

vec3 srgbToLinear(vec3 color) 
{
    vec3 linearColor;
    linearColor.r = srgbToLinear(color.r);
    linearColor.g = srgbToLinear(color.g);
    linearColor.b = srgbToLinear(color.b);
    return linearColor;
}

float linearToSRGB(float L) 
{
    if (L <= 0.0031308) 
    {
        return L * 12.92;
    } 
    else 
    {
        return 1.055 * pow(L, 1.0 / 2.4) - 0.055;
    }
}

vec3 linearToSRGB(vec3 color) 
{
    vec3 srgbColor;
    srgbColor.r = linearToSRGB(color.r);
    srgbColor.g = linearToSRGB(color.g);
    srgbColor.b = linearToSRGB(color.b);
    return srgbColor;
}