// TODO: Indoor mask
uniform sampler2D StarfieldTexture;
uniform sampler2D GradientTexture;
uniform samplerCube unSkyboxCube;
uniform vec2 ScreenResolution;

// Lighting uniforms (MaterialUtils::uploadTonemapUniforms)
uniform vec2 unExposure;
uniform float unLightingSplit;
uniform vec2 unGamma;
uniform vec2 unSunIntensity;

#include "../GlobalUbo.glsl"
#include "util/gamma.glsl"

in vec2 fUV;
in vec3 fPosition;
in vec3 fSkyVector;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out float oRoughness;

vec3 getCurrentSunColor(int preset, vec3 sunColor, float sunIntensity) {
	float sunIntensityAdjusted = max(sunIntensity * unSunIntensity[preset], 0.0);
	float lightTotal = sunIntensityAdjusted;// + unAmbient[preset];
    return lightTotal * sunColor;
}

vec3 getAdditiveFromSun(float sunVal, float sunAngle, vec3 color, float size) {
    // Glow
    float additive = pow(sunAngle, 4.0) * max(pow(sunVal, 1.0 - size * 40.0), 0.0);
	// Sky sun glow + sun texture
	additive += (sunAngle * 0.5 + max(pow(sunAngle - (1.0 - size), 0.3), 0.0) * 2.0);
    return additive * color;
}

void main() {
    const int preset = int(step(unLightingSplit, gl_FragCoord.x / ScreenResolution.x));
    
	// Add UV based on rotation so the sky rotates (tiling)
	// Zangle is between 0 and 2PI
	float SunIntensity = max(SunHeight, 0.0);
	vec4 starTextureColor = texture(StarfieldTexture, fUV).rgba;
	float positionInput = (fPosition.z - fPosition.x - fPosition.y) * 700.0;
	float sparkle = (sin(Time * 1.25 + positionInput) + 1.3) * 0.434782;
	float starIntensity = pow(1.0 - SunIntensity, 20.0);
    vec3 color = starTextureColor.rgb * starTextureColor.a * sparkle * starIntensity;
    
    // Clouds
    
    // TODO: Fix stars

    // NEW CUBEMAP
    vec3 color2 = gammaDecode(texture(unSkyboxCube, fSkyVector).rgb, unGamma[preset]);
    
    float tmpSkyOpacity = 1.0 - step(0.25, color2.r);
    color.rgb = color.rgb * tmpSkyOpacity;
    
    
    
    // Apply exposure (0.5 are there to soften effect)
    color2 = vec3(1.0) - exp(-color2 * (unExposure[preset] + 0.5) * 0.5);
    
    // SKYLIGHT
    vec3 sunColor = getCurrentSunColor(preset, SunColor, SunHeight);
    
    vec3 pixelColor = color2;
    pixelColor = sunColor * pixelColor;
    
    // World space ray
	vec3 rayWorld = normalize(fSkyVector);
	// Get Angle
    float DIFF = 0.02;
	float sunAngle = max(pow(dot(normalize(SunPosition - vec3(0.0, DIFF, DIFF)), rayWorld), 64.0), 0.0);
    float sunAngle2 = max(pow(dot(normalize(SunPosition + vec3(0.0, DIFF, DIFF)), rayWorld), 64.0), 0.0);
	
	// Sun Glow
	float sunVal = max(SunIntensity, 0.0);
    pixelColor += getAdditiveFromSun(sunVal, sunAngle, vec3(0.5, 0.55, 1.0), 0.006); // BLUE SUN
    pixelColor += getAdditiveFromSun(sunVal, sunAngle2, vec3(1.0, 0.55, 0.5), 0.01); // RED SUN
    
    pixelColor = sunColor * unExposure[preset] * 0.0001 + pixelColor;
    float dot = max(dot(fSkyVector, SunPosition) - 0.9, 0.0) * 30.0;
    
    // END SKYLIGHT
    
    // SKY HAZE
    vec3 hazeColor = texture(GradientTexture, vec2(0.5, sunVal)).rgb;
    float hazeIntensity = 1.0 - max(rayWorld.z, 0.0);
    hazeIntensity = pow(hazeIntensity, 5.0) * sunVal; // Decrease haze as sun drops
    pixelColor = mix(pixelColor, hazeColor, hazeIntensity);
    
    
    // TODO: Remove old
    oColor.rgb = color.rgb + gammaEncode(pixelColor, unGamma[preset]);
    oColor.a = 1.0;
    
    oNormal.rgb = vec3(0.0);
    
    oRoughness = 1.0;
}