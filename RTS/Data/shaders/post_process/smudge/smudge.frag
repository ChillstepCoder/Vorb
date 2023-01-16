uniform sampler2D unAlbedoFbo;
uniform sampler2D unNormalFbo;
uniform sampler2D unDepthFbo;
uniform vec2 unScreenResolution;
uniform vec2 unDirection;
uniform vec2 unCameraZRange;
uniform float unNormalThreshold;
uniform float unDepthThreshold;
uniform int unShowEdges;
uniform int unShowVariance;

#include "GlobalUbo.glsl"

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * unCameraZRange.x * unCameraZRange.y / (unCameraZRange.y + unCameraZRange.x - zn * (unCameraZRange.y - unCameraZRange.x));
}

vec3 sampleNormalAndIncrementTotalWeight(sampler2D image, vec2 uv, float weight, inout float weightTotal, float baseDepth, inout vec3 avgColor) {

    const float depth = linearizeDepth(texture(unDepthFbo, uv).r);
    if (abs(depth - baseDepth) < unDepthThreshold) {
        vec3 color = texture2D(unAlbedoFbo, uv).rgb;
        vec3 normal = texture2D(image, uv).rgb;
        float isValid = length(normal);
        weightTotal += weight * isValid;
        avgColor += color.rgb * weight * isValid;
        return normal.rgb * weight * isValid;
    } else {
        return vec3(0.0);
    }
}

vec3 getAverageNormalAndColor(sampler2D image, vec2 uv, vec2 resolution, vec2 direction, inout vec3 avgColor) {
  vec3 nAvg = vec3(0.0);
  avgColor = vec3(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  
  const float baseDepth = linearizeDepth(texture(unDepthFbo, uv).r);
  
  float weightTotal = 0.0;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv, 0.1964825501511404, weightTotal, baseDepth, avgColor);
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv + (off1 / resolution), 0.2969069646728344, weightTotal, baseDepth, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv - (off1 / resolution), 0.2969069646728344, weightTotal, baseDepth, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv + (off2 / resolution), 0.09447039785044732, weightTotal, baseDepth, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv - (off2 / resolution), 0.09447039785044732, weightTotal, baseDepth, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv + (off3 / resolution), 0.010381362401148057, weightTotal, baseDepth, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv - (off3 / resolution), 0.010381362401148057, weightTotal, baseDepth, avgColor).rgb;
  
  avgColor /= weightTotal;
  nAvg /= weightTotal;
  return nAvg;
}

in vec2 fUV;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;

float getNormalVariance(vec3 n1, vec3 n2) {
    return length(n1 - n2);
}

void main() {
    vec3 baseNormal = texture(unNormalFbo, fUV).rgb;
    vec3 baseColor = texture(unAlbedoFbo, fUV).rgb;
    vec3 avgColor;
    float normLength = length(baseNormal);
    if (normLength > 0.0) {
        vec3 avgNormal = getAverageNormalAndColor(unNormalFbo, fUV, unScreenResolution, unDirection, avgColor);
        if (unShowVariance == 1) {
            oColor.rgb = vec3(getNormalVariance(baseNormal, avgNormal) * 20.0, 0.0, 0.0);
            oNormal = avgNormal;
        } else if (getNormalVariance(avgNormal, baseNormal) > unNormalThreshold) {
            if (unShowEdges == 1) {
                oColor.rgb = vec3(1.0, 0.0, 0.0);
            } else {
                oColor.rgb = avgColor;
            }
            oNormal = avgNormal;
        } else {
            oColor.rgb = baseColor;
            oNormal = baseNormal;
        }
    } else {
        oColor.rgb = baseColor;
        oNormal.rgb = vec3(0.0);
    }
    oColor.a = 1.0;
    

}