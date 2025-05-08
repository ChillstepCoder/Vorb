
float getAmbientFactor(int preset, vec2 ambient, float sunHeight) {
    const float minSunHeight = 0.1;
    const float sunExponent = 2.0;
    
    float sunFactor = pow(max(sunHeight, minSunHeight), sunExponent);
    return ambient[preset] * sunFactor;
}