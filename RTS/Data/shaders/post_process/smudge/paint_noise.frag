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
uniform vec2 unCameraZRange;

const float DEPTH_THRESHOLD = 1.0;

#include "GlobalUbo.glsl"
#include "util/depth.glsl"
#include "util/gaussian_blur.glsl"

//vec3 worldPosFromDepth(float depth, vec2 fboUV, mat4 inverseV, mat4 inverseP)

in vec2 fUV;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;


vec3 sampleNormalAndIncrementTotalWeight(vec2 uv, float weight, inout float weightTotal, float baseDepth, inout vec4 avgColor) {

    const float depth = linearizeDepth(texture(unDepthFbo, uv).r, unCameraZRange);
    if (abs(depth - baseDepth) < DEPTH_THRESHOLD) {
        vec4 color = texture2D(unAlbedoFbo, uv);
        vec3 normal = texture2D(unNormalFbo, uv).rgb;
        float isValid = step(0.01, dot(normal, normal)); // TODO: isValid IS needed, but why? Shouldnt the input buffer always have valid normals?
        weightTotal += weight * isValid;
        avgColor += color * weight * isValid;
        return normal.rgb * weight * isValid;
    } else {
        return vec3(0.0);
    }
}

vec3 getAverageNormalAndColor(vec2 uv, vec2 resolution, vec2 direction, vec3 baseNormal, vec4 baseColor, inout vec4 avgColor) {

  float weightTotal = 0.1964825501511404;
  vec3 nAvg = baseNormal * weightTotal;
  avgColor = baseColor * weightTotal;
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  
  const float baseDepth = linearizeDepth(texture(unDepthFbo, uv).r, unCameraZRange);
  
  nAvg += sampleNormalAndIncrementTotalWeight(uv + (off1 / resolution), 0.2969069646728344, weightTotal, baseDepth, avgColor);
  nAvg += sampleNormalAndIncrementTotalWeight(uv - (off1 / resolution), 0.2969069646728344, weightTotal, baseDepth, avgColor);
  nAvg += sampleNormalAndIncrementTotalWeight(uv + (off2 / resolution), 0.09447039785044732, weightTotal, baseDepth, avgColor);
  nAvg += sampleNormalAndIncrementTotalWeight(uv - (off2 / resolution), 0.09447039785044732, weightTotal, baseDepth, avgColor);
  nAvg += sampleNormalAndIncrementTotalWeight(uv + (off3 / resolution), 0.010381362401148057, weightTotal, baseDepth, avgColor);
  nAvg += sampleNormalAndIncrementTotalWeight(uv - (off3 / resolution), 0.010381362401148057, weightTotal, baseDepth, avgColor);
  
  avgColor /= weightTotal;
  nAvg /= weightTotal;
  //Normalize normal in the -1 - 1 range
  nAvg = (normalize(nAvg * 2.0 - 1.0) * 0.5) + 0.5;
  return nAvg;
}

void main() {
    const vec3 relativeWorldPos = worldPosFromDepth(texture(unDepthFbo, fUV).r, fUV, InverseV, InverseP);
    const vec3 worldPos = relativeWorldPos + CameraPos;
    
    float distance = length(relativeWorldPos);
    float noiseScale = min(distance * 0.01, 1.0);
    vec4 avgColor;
    float noiseVal = texture(PerlinNoise, worldPos.xy * unNoiseFrequency).r + unNoiseOffset;
    //noiseVal = smoothstep(0.0, 1.0, noiseVal);
    noiseVal = clamp(noiseVal * unNoiseAmplitude * noiseScale, 0.0, 1.0);
    
    vec3 baseNormal = texture(unNormalFbo, fUV).rgb;
    vec4 baseColor = texture(unAlbedoFbo, fUV);
    
    // Tweak this for different blur shapes
    vec2 stylizedOffset = vec2(sin(worldPos.x * 0.5), cos(worldPos.y * 0.5));
    vec3 avgNormal = getAverageNormalAndColor(fUV, unScreenResolution, unDirection + stylizedOffset, baseNormal, baseColor, avgColor);
    
    vec4 newAlbedo = mix(baseColor, avgColor, noiseVal);
    vec3 newNormal = mix(baseNormal, avgNormal, noiseVal);
   
    oColor = newAlbedo;
    oNormal.rgb = newNormal;
    
    if (unDebugRender == 1) {
        oColor.rg = vec2(noiseVal);
    }
}