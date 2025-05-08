
uniform vec2 unHazeDivisor;
uniform vec2 unHazeExponent;

// Camera position is always at 0
vec3 applyHaze(vec3 inputColor, vec3 cameraRelativePos, int preset, vec3 hazeColor) {
   
    float sunIntensity = max(SunHeight, 0.0);
    
    float linearDistance = length(cameraRelativePos);
    float hazeDistance = linearDistance / unHazeDivisor[preset];
	float hazeDepth = pow(min(hazeDistance, 1.0), unHazeExponent[preset]);
	float depthHaze = hazeDepth * (pow(sunIntensity, 0.5));
	// Day Haze
    
	vec3 pixelColor = mix(inputColor, hazeColor, depthHaze);

	// Night Haze, darken everything at night
    const float NIGHT_DARKNESS_FACTOR = 0.8;
    const float NIGHT_THRESHOLD_FACTOR = 0.005;
    const float NIGHT_HAZE_DIVISOR = 300.0;
    const float MIN_AMBIENT = 0.2;
    float divisor = NIGHT_HAZE_DIVISOR + sunIntensity * 6000.0;
    const float nightHazeDistance = linearDistance / divisor;
	float nightHaze = 1.0 - pow(nightHazeDistance * (1.0 - pow(sunIntensity, NIGHT_THRESHOLD_FACTOR)), NIGHT_DARKNESS_FACTOR);
    nightHaze = max(nightHaze, MIN_AMBIENT);
	pixelColor *= nightHaze;
    
    return pixelColor;
}