
uniform sampler2D FboDepth;
uniform sampler2D ShadowTexture;
uniform vec2 ScreenResolution;
uniform sampler2D unSurfaceDistort;
uniform sampler2D unSurfaceNoise;
uniform sampler2D unOilCanvas;
uniform sampler2D unHsvJitter;
uniform sampler2D GradientTexture;
uniform float unWaterMetallic;
uniform float unWaterRoughness;

uniform float unSurfaceDistortAmount = 0.27;
uniform float unSurfaceMoveSpeed = 0.03;
uniform vec2 unFoamDistanceRange = vec2(0.04, 0.4);
uniform float unSurfaceNoiseCutoff = 0.777;
uniform float unSmoothstepAA = 0.01;
uniform float unColorNoiseIntensity;
uniform vec4 unFoamColor;
uniform float unDistortTiling;
uniform float unNoiseTiling;
uniform float unSnowLevel;

// Lighting uniforms (MaterialUtils::uploadLightingUniforms)
uniform vec2 unAmbient;
uniform vec2 unSunIntensity;
uniform vec2 unExposure;
uniform float unLightingSplit;

#include "util/pbr.glsl"
#include "util/ambient.glsl"
#include "util/haze.glsl"
#include "util/gamma.glsl"
#include "util/depth.glsl"

// TODO: Move out?
vec3 getCurrentSunColor(int preset, vec3 sunColor, float sunIntensity) {
	float sunIntensityAdjusted = max(sunIntensity * unSunIntensity[preset], 0.0);
	float lightTotal = sunIntensityAdjusted;// + unAmbient[preset];
    return lightTotal * sunColor;
}

vec4 computeWaterColor(vec3 cameraRelativePos, vec2 worldUV, float depthDiff, float cameraDist, vec4 waterColor) {
    
    // For increasing foam at low camera angles
    float cameraZDegree = clamp(CameraFront.z + 0.3, 0.0, 1.0);

    float foamDistance = mix(unFoamDistanceRange.x, unFoamDistanceRange.y, cameraZDegree);
    float foamDepthDiff = clamp(depthDiff / foamDistance, 0.0, 1.0);
    float surfaceNoiseCutoff = foamDepthDiff * unSurfaceNoiseCutoff;
    
    // get difference in depth
    float maxDepthDiff = 0.8;
    float unclampedDepthDiff = depthDiff / maxDepthDiff;
    depthDiff = clamp(unclampedDepthDiff, 0.0, 1.0);
    
    vec2 uvOffset = vec2(vec2(1.0) - smoothstep(0.0, 1.0, surfaceNoiseCutoff)) * 0.1;
    
    vec2 timeOffset = vec2(Time) * unSurfaceMoveSpeed;
    vec2 distortSample = texture(unSurfaceDistort, worldUV.xy * unDistortTiling + uvOffset).rg * unSurfaceDistortAmount;
    // Add a finer detail layer (need to apply to normal too)
    //distortSample += texture(unSurfaceDistort, worldUV.xy * unDistortTiling * 4.0 + uvOffset).rg * unSurfaceDistortAmount;
    
    vec2 noiseUV = worldUV + timeOffset + distortSample;
    noiseUV += uvOffset;
     
    // TODO tex2dproj?
    float surfaceNoiseSample = texture(unSurfaceNoise, noiseUV * unNoiseTiling).r;


    float surfaceNoise = smoothstep(surfaceNoiseCutoff - unSmoothstepAA, surfaceNoiseCutoff + unSmoothstepAA, surfaceNoiseSample);
    vec3 surfaceNoiseColor = unFoamColor.xyz * surfaceNoise;

    // Reduce transparency at distance
    vec4 outColor = waterColor + vec4(surfaceNoiseColor, 0.0);
    float cameraDistAlpha = cameraDist * 0.01;
    float depthAlphaAdd = pow(unclampedDepthDiff * 0.03, 2.0);
    outColor.a = clamp(mix(outColor.a, 1.0, cameraDistAlpha) + depthAlphaAdd, 0.0, 1.0);
    
    // Oil paint
    float oilDistortVal = 0.3;
    float oilMoveSpeed = 0.5;
    vec2 oilUV = worldUV + vec2(Time) * unSurfaceMoveSpeed * oilMoveSpeed + distortSample * oilDistortVal;
    vec3 oilColor = texture(unOilCanvas, oilUV * 2.0 + uvOffset).rgb;
    outColor.rgb = outColor.rgb * oilColor;
    
    // Color noise
    vec2 colorNoiseUV = worldUV + vec2(Time) * unSurfaceMoveSpeed * oilMoveSpeed + distortSample * oilDistortVal;
    vec3 colorJitter = texture(unHsvJitter, colorNoiseUV * 0.02 + uvOffset).rgb;
    outColor.rgb += cos(colorJitter * 15.0) * unColorNoiseIntensity;
    
    vec2 normalSample = texture(unSurfaceDistort, worldUV.xy * unDistortTiling + timeOffset + uvOffset).rg * unSurfaceDistortAmount;
    vec3 normal = normalize(vec3((normalSample.xy - 0.14) * 2.0, 0.5));
    
    // Lighting and shadow
    vec2 fboUV = gl_FragCoord.xy / ScreenResolution;
	float shadow = texture(ShadowTexture, fboUV).r;
    
    const int preset = int(step(unLightingSplit, fboUV.x));
    // PBR
    vec3 sunColor = getCurrentSunColor(preset, SunColor, SunHeight);
    
    // Ice
    outColor.rgb = mix(outColor.rgb, vec3(0.9, 0.9, 1.0), unSnowLevel);
    
    //oColor.rgb *= 2.0;
    float ambient = getAmbientFactor(preset, unAmbient, SunHeight);
    outColor.rgb = PBR(cameraRelativePos, outColor.rgb, normal, unWaterMetallic, unWaterRoughness, 1.0, shadow, ambient, unExposure[preset], sunColor, SunPosition, vec3(0.0));
    vec3 hazeColor = texture(GradientTexture, vec2(0.5, max(SunHeight, 0.0))).rgb;
    outColor.rgb = applyHaze(outColor.rgb, cameraRelativePos, preset, hazeColor);
    outColor.rgb = gammaEncode(outColor.rgb, 2.2);
    outColor.a *= foamDepthDiff;
    

    
    return outColor;
}