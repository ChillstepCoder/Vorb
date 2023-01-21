uniform sampler2D unTextureAlbedo;
uniform sampler2D unTextureNormals;
uniform sampler2D unTextureDepth;
uniform sampler2D unTextureShadow;

uniform sampler2D GradientTexture;
uniform vec3 ShadowColor;

// Lighting uniforms (MaterialUtils::uploadLightingUniforms)
uniform vec2 unHazeExponent;
uniform ivec2 unLightingModel;
uniform vec2 unHazeDivisor;
uniform vec2 unAmbient;
uniform vec2 unSunIntensity;
uniform float unLightingSplit;

in vec2 fUV;
layout (location = 0) out vec3 oColor;

#include "GlobalUbo.glsl"
#include "util/depth.glsl"
#include "util/lighting.glsl"

vec3 getCurrentSunColor(int preset, vec3 sunColor, float sunIntensity) {
	float sunIntensityAdjusted = max(sunIntensity * unSunIntensity[preset], 0.0);
	float lightTotal = sunIntensityAdjusted + unAmbient[preset];
    return lightTotal * sunColor;
}

vec3 lightPixelSunlight(vec3 albedo, vec3 normal, vec3 worldPos, vec2 screenUV, float roughness, float isSky, float shadow, mat4 inverseVP, vec3 sunColor, float sunIntensity, vec3 sunPosition) {
    
    // Split view for light presets
    int preset = int(step(unLightingSplit, screenUV.x));

	float isGround = 1.0 - isSky;

    // =====================================================
	// ==                     Sunlight color              ==
	// =====================================================
	
    sunColor = getCurrentSunColor(preset, sunColor, sunIntensity);
    // Clamp sky light total
    vec3 pixelColor = sunColor * albedo;
	
	// =====================================================
	// ==                     SUN                         ==
	// =====================================================
	// World space ray
	vec4 rayClip = vec4(screenUV.x * 2.0 - 1.0, screenUV.y * 2.0 - 1.0, -1.0, 1.0);
	vec4 rayWorld = inverseVP * rayClip;
	// Get Angle
	float sunAngle = max(pow(dot(sunPosition, normalize(rayWorld.xyz)), 64.0), 0.0);
	
	// Sun Glow
	float hazeIntensity = max(sunIntensity, 0.0);
	pixelColor += sunAngle * max(pow(hazeIntensity, 0.15), 0.0);
	// Sky sun glow + sun texture
	pixelColor += isSky * (sunAngle * 0.5 + max(pow(sunAngle - 0.95, 0.3), 0.0) * 2.0);
	
	// Sun phong
    float PHONG_AMBIENT = 0.5;
    if (unLightingModel[preset] == 0) {
        // Phong
        pixelColor = computePhongHDR(worldPos, pixelColor, normal, sunPosition, PHONG_AMBIENT, roughness, shadow);
    } else {
        // Blinn phong
        pixelColor = computeBlinnPhongHDR(worldPos, pixelColor, normal, sunPosition, PHONG_AMBIENT, roughness, shadow);
    }
	
	// =====================================================
	// ==                     SHADOW                      ==
	// =====================================================
	float shadowMult = shadow * 0.5 * sunIntensity;
	pixelColor = mix(pixelColor, pixelColor * ShadowColor, shadowMult);
	
	// =====================================================
	// ==                     SSAO                        ==
	// =====================================================
	//float ssao = texture(SSAOTexture, screenUV).r;
	//pixelColor = mix(SSAOColor, pixelColor, ssao);
	//pixelColor = pixelColor * 0.00001 + vec3(ssao);
    
    // =====================================================
	// ==                     HAZE                        ==
	// =====================================================
    float trueWorldZ = worldPos.z + CameraPos.z;
    
    float zAdjust = step(0.0, -trueWorldZ) * -trueWorldZ;
    vec3 hazeWorldPos = worldPos + vec3(0.0, 0.0, zAdjust);
    
    float hazeDistance = length(hazeWorldPos) / unHazeDivisor[preset] + unHazeDivisor[preset] * isSky;
	float hazeDepth = pow(min(hazeDistance, 1.0), unHazeExponent[preset]);
	float depthHaze = hazeDepth * (pow(hazeIntensity, 0.5));
	vec2 adjustedUV = screenUV;
	adjustedUV.y = hazeIntensity;
	vec3 sunTextureColor = texture(GradientTexture, adjustedUV).rgb;
	// Day Haze
	pixelColor = pixelColor * (1.0 - depthHaze * isGround) + depthHaze * sunTextureColor;

	// Night Haze
	float nightHaze = 1.0 - hazeDepth * isGround * (1.0 - pow(hazeIntensity, 0.2));
	pixelColor *= nightHaze;
    
    return pixelColor;
}


void main() {

	float depth = texture(unTextureDepth, fUV).r;
    vec3 worldPos = worldPosFromDepth(depth, fUV, InverseV, InverseP);

	float isSky = step(0.999999999, depth);
	float shadow = texture(unTextureShadow, fUV).r;
	vec3 albedoColor = texture(unTextureAlbedo, fUV).rgb;
    vec3 normal = texture(unTextureNormals, fUV).rgb;
	float roughness = 0.25;
	roughness = max(roughness, isSky);
	normal = normal * 2.0 - 1.0;
    
	oColor = lightPixelSunlight(albedoColor, normal, worldPos, fUV, roughness, isSky, shadow * 0.5, InverseVP, SunColor, SunHeight, SunPosition); // TODO: NOTE THIS 0.5 IS HARD CODED!

}