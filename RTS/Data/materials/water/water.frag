#include "../GlobalUbo.glsl"

uniform vec3 DebugColor1;
uniform sampler2D FboDepth;
uniform sampler2D FboNormals;
uniform vec2 ScreenResolution;
uniform sampler2D unSurfaceDistort;
uniform sampler2D unSurfaceNoise;

const float SURFACE_DISTORD_AMOUNT = 0.27;
const vec2 SURFACE_MOVE_SPEED = vec2(0.03);
const vec2 FOAM_DISTANCE_RANGE = vec2(0.4, 0.04);
const float SURFACE_NOISE_CUTOFF = 0.777;
const float SMOOTHSTEP_AA = 0.01;

in vec3 fPosition;
in vec2 fUV;
in float fDepth;
in float fWaveHeight;

layout (location = 0) out vec4 oColor; // TODO: vec3
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * CameraZRange.x * CameraZRange.y / (CameraZRange.y + CameraZRange.x - zn * (CameraZRange.y - CameraZRange.x));
}

void main() {
	
    vec2 fboUV = gl_FragCoord.xy / ScreenResolution;
	float depth = texture2D(FboDepth, fboUV.xy).r;
    
    vec4 shallowColor = vec4(0.325, 0.807, 0.971, 0.725);
    vec4 deepColor = vec4(0.086, 0.407, 1, 0.749);
    vec4 foamColor = vec4(1.0, 1.0, 1.0, 1.0);
    
    // get difference in depth
    float maxDepthDiff = 0.8;
    float depthDiff = linearizeDepth(depth) - linearizeDepth(gl_FragCoord.z);
    depthDiff = clamp(depthDiff / maxDepthDiff, 0.0, 1.0);
    vec4 waterColor = mix(shallowColor, deepColor, depthDiff);
    
    vec2 distortSample = texture(unSurfaceDistort, fUV.xy).rg * SURFACE_DISTORD_AMOUNT;
    
    vec2 noiseUV = vec2((fUV.x + Time * SURFACE_MOVE_SPEED.x) + distortSample.x, (fUV.y + Time * SURFACE_MOVE_SPEED.y) + distortSample.y);
     
    // TODO tex2dproj?
    float surfaceNoiseSample = texture(unSurfaceNoise, noiseUV).r;

// ERROR READING AND WRITING TO SAME NORMAL TEXTURE
    vec3 existingNormal = normalize(texture(FboNormals, fboUV).rgb * 2.0 - 1.0);
    float normalDot = clamp(dot(existingNormal, vec3(0.0, 0.0, 1.0)), 0.0, 1.0);

    float foamDistance = mix(FOAM_DISTANCE_RANGE.y, FOAM_DISTANCE_RANGE.x, normalDot);
    float foamDepthDiff = clamp(depthDiff / foamDistance, 0.0, 1.0);
    float surfaceNoiseCutoff = foamDepthDiff * SURFACE_NOISE_CUTOFF;

    float surfaceNoise = smoothstep(surfaceNoiseCutoff - SMOOTHSTEP_AA, surfaceNoiseCutoff + SMOOTHSTEP_AA, surfaceNoiseSample);
    vec4 surfaceNoiseColor = foamColor * surfaceNoise;

    oColor = waterColor + surfaceNoiseColor;
    
    oNormal.rgb = (normalize(vec3(distortSample.xy, 1.0)) + 1.0) * 0.5;
    oNormal.a = 1.0;
	oRoughness.r = 0.0;
	oRoughness.a = 1.0;
}