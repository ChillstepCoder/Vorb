vec3 gammaEncode(vec3 color, float gamma) 
{
    return pow(color, vec3(1.0 / gamma));
}

vec3 gammaDecode(vec3 color, float gamma) 
{
    return pow(color, vec3(gamma));
}

vec3 gammaCorrection(vec3 color, float gamma) 
{
    return gammaEncode(color, gamma);
}