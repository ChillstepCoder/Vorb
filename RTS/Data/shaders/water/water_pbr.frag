#include "GlobalUbo.glsl"

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
uniform vec4 unShallowColor;
uniform vec4 unDeepColor;
uniform vec4 unFoamColor;
uniform float unDistortTiling;
uniform float unNoiseTiling;

// Lighting uniforms (MaterialUtils::uploadLightingUniforms)
uniform vec2 unAmbient;
uniform vec2 unSunIntensity;
uniform vec2 unExposure;
uniform float unLightingSplit;

in vec3 fPosition;
in vec2 fUV;
in float fDepth;
in float fCameraDist;

#include "util/pbr.glsl"
#include "util/ambient.glsl"
#include "util/haze.glsl"
#include "util/gamma.glsl"
#include "util/depth.glsl"

layout (location = 0) out vec4 oColor;

vec3 getCurrentSunColor(int preset, vec3 sunColor, float sunIntensity) {
	float sunIntensityAdjusted = max(sunIntensity * unSunIntensity[preset], 0.0);
	float lightTotal = sunIntensityAdjusted;// + unAmbient[preset];
    return lightTotal * sunColor;
}

void main() {
	
    vec2 fboUV = gl_FragCoord.xy / ScreenResolution;
	float depth = texture2D(FboDepth, fboUV.xy).r;
    
    // get difference in depth
    float maxDepthDiff = 0.8;
    float linFragDepth = linearizeDepth(gl_FragCoord.z, CameraZRange);
    float depthDiff = linearizeDepth(depth, CameraZRange) - linFragDepth;
    float unclampedDepthDiff = depthDiff / maxDepthDiff;
    depthDiff = clamp(unclampedDepthDiff, 0.0, 1.0);
    vec4 waterColor = mix(unShallowColor, unDeepColor, depthDiff);
    
    vec2 timeOffset = vec2(Time) * unSurfaceMoveSpeed;
    vec2 distortSample = texture(unSurfaceDistort, fUV.xy * unDistortTiling).rg * unSurfaceDistortAmount;
    // Add a finer detail layer (need to apply to normal too)
    //distortSample += texture(unSurfaceDistort, fUV.xy * unDistortTiling * 4.0).rg * unSurfaceDistortAmount;
    
    vec2 noiseUV = fUV + timeOffset + distortSample;
     
    // TODO tex2dproj?
    float surfaceNoiseSample = texture(unSurfaceNoise, noiseUV * unNoiseTiling).r;

// ERROR READING AND WRITING TO SAME NORMAL TEXTURE
    
    // For increasing foam at low camera angles
    float cameraZDegree = clamp(CameraFront.z + 0.3, 0.0, 1.0);

    float foamDistance = mix(unFoamDistanceRange.x, unFoamDistanceRange.y, cameraZDegree);
    float foamDepthDiff = clamp(depthDiff / foamDistance, 0.0, 1.0);
    float surfaceNoiseCutoff = foamDepthDiff * unSurfaceNoiseCutoff;

    float surfaceNoise = smoothstep(surfaceNoiseCutoff - unSmoothstepAA, surfaceNoiseCutoff + unSmoothstepAA, surfaceNoiseSample);
    vec3 surfaceNoiseColor = unFoamColor.xyz * surfaceNoise;

    // Reduce transparency at distance
    oColor = waterColor + vec4(surfaceNoiseColor, 0.0);
    float cameraDistAlpha = fCameraDist * 0.01;
    float depthAlphaAdd = pow(unclampedDepthDiff * 0.03, 2.0);
    oColor.a = clamp(mix(oColor.a, 1.0, cameraDistAlpha) + depthAlphaAdd, 0.0, 1.0);
    
    // Oil paint
    float oilDistortVal = 0.3;
    float oilMoveSpeed = 0.5;
    vec2 oilUV = fUV + vec2(Time) * unSurfaceMoveSpeed * oilMoveSpeed + distortSample * oilDistortVal;
    vec3 oilColor = texture(unOilCanvas, oilUV * 2.0).rgb;
    oColor.rgb = oColor.rgb * oilColor;
    
    // Color noise
    vec2 colorNoiseUV = fUV + vec2(Time) * unSurfaceMoveSpeed * oilMoveSpeed + distortSample * oilDistortVal;
    vec3 colorJitter = texture(unHsvJitter, colorNoiseUV * 0.02).rgb;
    oColor.rgb += cos(colorJitter * 15.0) * unColorNoiseIntensity;
    
    vec2 normalSample = texture(unSurfaceDistort, fUV.xy * unDistortTiling + timeOffset).rg * unSurfaceDistortAmount;
    vec3 normal = normalize(vec3((normalSample.xy - 0.14) * 2.0, 0.5));
    
    // Lighting and shadow
	float shadow = texture(ShadowTexture, fboUV).r;
    
    const int preset = int(step(unLightingSplit, fboUV.x));
    // PBR
    vec3 sunColor = getCurrentSunColor(preset, SunColor, SunHeight);
    
    
    //oColor.rgb *= 2.0;
    float ambient = getAmbientFactor(preset, unAmbient, SunHeight);
    oColor.rgb = PBR(fPosition, oColor.rgb, normal, unWaterMetallic, unWaterRoughness, 1.0, shadow, ambient, unExposure[preset], sunColor, SunPosition, vec3(0.0));
    vec3 hazeColor = texture(GradientTexture, vec2(0.5, max(SunHeight, 0.0))).rgb;
    oColor.rgb = applyHaze(oColor.rgb, fPosition, preset, hazeColor);
    oColor.rgb = gammaEncode(oColor.rgb, 2.2);
    
    //oColor.rgb = 0.0001 * oColor.rgb + foamDepthDiff;
    //oColor.a = 0.0001 * oColor.a + 1.0;
    oColor.a *= foamDepthDiff;
}