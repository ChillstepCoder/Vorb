uniform sampler2D unAlbedoFbo;
uniform sampler2D unNormalFbo;
uniform sampler2D unDepthFbo;
uniform sampler2D PerlinNoise;
uniform int unDebugRender = 0; 
uniform float unNoiseOffset;
uniform float unNoiseFrequency;
uniform float unNoiseAmplitude;
uniform vec2 unScreenResolution;
uniform vec2 unDirection;

#include "GlobalUbo.glsl"
#include "util/depth.glsl"
#include "util/gaussian_blur.glsl"

//vec3 worldPosFromDepth(float depth, vec2 fboUV, mat4 inverseV, mat4 inverseP)

in vec2 fUV;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;


void main() {
    const vec3 relativeWorldPos = worldPosFromDepth(texture(unDepthFbo, fUV).r, fUV, InverseV, InverseP);
    const vec3 worldPos = relativeWorldPos + CameraPos;
    
    float distance = length(relativeWorldPos);
    float noiseScale = min(distance * 0.02, 1.0);
    
    float noiseVal = texture(PerlinNoise, worldPos.xy * unNoiseFrequency).r + unNoiseOffset;
    //noiseVal = smoothstep(0.0, 1.0, noiseVal);
    noiseVal = clamp(noiseVal * unNoiseAmplitude * noiseScale, 0.0, 1.0);
    
    vec3 newAlbedo = blur9(unAlbedoFbo, fUV, unScreenResolution, unDirection * noiseVal).rgb;
    vec3 newNormal = blur9(unNormalFbo, fUV, unScreenResolution, unDirection * noiseVal).rgb;
   
    oColor.rgb = newAlbedo;
    oNormal.rgb = newNormal;
    oColor.a = 1.0; // TODO GET RID OF THIS
    
    if (unDebugRender == 1) {
        oColor.rg = vec2(noiseVal);
    }
}