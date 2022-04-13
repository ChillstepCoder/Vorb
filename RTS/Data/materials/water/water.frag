#include "GlobalUbo.glsl"

uniform sampler2DArray Atlas; // THIS IS FOR COLOR GRADIENT, TODO: Non atlas? hmm

uniform sampler2D FboDepth;
uniform sampler2D ShadowTexture;
uniform vec2 ScreenResolution;
uniform sampler2D unSurfaceDistort;
uniform sampler2D unSurfaceNoise;
uniform sampler2D unOilCanvas;
uniform sampler2D unHsvJitter;

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

in vec3 fPosition;
in vec2 fUV;
in float fDepth;
in float fCameraDist;

#include "lighting/scene_lighting.glsl"

layout (location = 0) out vec4 oColor;

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * CameraZRange.x * CameraZRange.y / (CameraZRange.y + CameraZRange.x - zn * (CameraZRange.y - CameraZRange.x));
}


void main() {
	
    vec2 fboUV = gl_FragCoord.xy / ScreenResolution;
	float depth = texture2D(FboDepth, fboUV.xy).r;
    
    // get difference in depth
    float maxDepthDiff = 0.8;
    float linFragDepth = linearizeDepth(gl_FragCoord.z);
    float depthDiff = linearizeDepth(depth) - linFragDepth;
    depthDiff = clamp(depthDiff / maxDepthDiff, 0.0, 1.0);
    vec4 waterColor = mix(unShallowColor, unDeepColor, depthDiff);
    
    vec2 distortSample = texture(unSurfaceDistort, fUV.xy * unDistortTiling).rg * unSurfaceDistortAmount;
    
    vec2 noiseUV = fUV + vec2(Time) * unSurfaceMoveSpeed + distortSample;
     
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
    oColor.a = clamp(mix(oColor.a, 1.0, fCameraDist * 0.02), 0.0, 1.0);
    
    // Oil paint
    float oilDistortVal = 0.3;
    float oilMoveSpeed = 0.5;
    vec2 oilUV = fUV + vec2(Time) * unSurfaceMoveSpeed * oilMoveSpeed + distortSample * oilDistortVal;
    vec3 oilColor = texture(unOilCanvas, oilUV * 2.0).rgb;
    oColor.rgb = oColor.rgb * oilColor;
    
    // Color noise
    vec2 colorNoiseUV = fUV + vec2(Time) * unSurfaceMoveSpeed * oilMoveSpeed + distortSample * oilDistortVal;
    vec3 colorJitter = texture(unHsvJitter, colorNoiseUV * 0.2).rgb;
    oColor.rgb += cos(colorJitter * 15.0) * unColorNoiseIntensity;
    
    vec3 normal = normalize(vec3((distortSample.xy - 0.1) * 2.0, 0.5));
    
    // Lighting and shadow
	float shadow = texture(ShadowTexture, fboUV).r;
    oColor.rgb = lightPixel(oColor.rgb, normal, fPosition, fboUV, 0.0, 0.0, shadow);
   
}