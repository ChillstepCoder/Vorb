#include "MaterialData.glsl"
#include "GlobalUbo.glsl"
#include "util/ambient.glsl"
#include "util/gamma.glsl"
#include "util/depth.glsl"
#include "util/haze.glsl"
#include "util/pbr.glsl"

uniform sampler2D unTextureAlbedo;
uniform sampler2D unTextureNormals;
uniform sampler2D unTextureRoughness;
uniform sampler2D unTextureDepth;
uniform sampler2D unTextureShadow;

uniform sampler2D GradientTexture;

// Lighting uniforms (MaterialUtils::uploadLightingUniforms)
uniform vec2 unGamma;
uniform vec2 unAmbient;
uniform vec2 unSunIntensity;
uniform vec2 unExposure;
uniform float unLightingSplit;

in vec2 fUV;
layout (location = 0) out vec3 oColor;

vec3 getCurrentSunColor(int preset, vec3 sunColor, float sunIntensity) {
	float sunIntensityAdjusted = max(sunIntensity * unSunIntensity[preset], 0.0);
	float lightTotal = sunIntensityAdjusted;// + unAmbient[preset];
    return lightTotal * sunColor;
}

void main() {
    const int preset = int(step(unLightingSplit, fUV.x));

	float depth = texture(unTextureDepth, fUV).r;
    vec3 worldPos = worldPosFromDepth(depth, fUV, InverseV, InverseP);

	float shadow = texture(unTextureShadow, fUV).r;
    
	vec4 albedoAo = texture(unTextureAlbedo, fUV).rgba;
    albedoAo.rgb = gammaDecode(albedoAo.rgb, unGamma[preset]);
    vec3 normal = texture(unTextureNormals, fUV).rgb;
	normal = normalize(normal * 2.0 - 1.0);
    vec2 metallicRoughness = texture(unTextureRoughness, fUV).rg;
    float ao = albedoAo.a;
    
    // PBR
    vec3 sunColor = getCurrentSunColor(preset, SunColor, SunHeight);
    float ambient = getAmbientFactor(preset, unAmbient, SunHeight);
    oColor.rgb = PBR(worldPos, albedoAo.rgb, normal, metallicRoughness.r, metallicRoughness.g, ao, shadow, ambient, unExposure[preset], sunColor, SunPosition, vec3(0.0));
    vec3 prevColor = oColor.rgb;
    // Haze (TODO: Final tonemap?)
    vec3 hazeColor = texture(GradientTexture, vec2(0.5, max(SunHeight, 0.0))).rgb;
    oColor.rgb = applyHaze(oColor.rgb, worldPos, preset, hazeColor);
    oColor.rgb = gammaEncode(oColor.rgb, unGamma[preset]);
}