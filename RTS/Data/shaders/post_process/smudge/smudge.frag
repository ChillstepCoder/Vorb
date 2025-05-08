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
#include "util/depth.glsl"


vec3 sampleNormalAndIncrementTotalWeight(vec2 uv, float weight, inout float weightTotal, float baseDepth, inout vec4 avgColor) {

    const float depth = linearizeDepth(texture(unDepthFbo, uv).r, unCameraZRange);
    if (abs(depth - baseDepth) < unDepthThreshold) {
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
  
  //nAvg += sampleNormalAndIncrementTotalWeight(uv, 0.1964825501511404, weightTotal, baseDepth, avgColor);
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

in vec2 fUV;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;

float getNormalVariance(vec3 n1, vec3 n2) {
    return length(n1 - n2);
}

void main() {
    vec3 baseNormal = texture(unNormalFbo, fUV).rgb;
    vec4 baseColor = texture(unAlbedoFbo, fUV);
    vec4 avgColor;
    // Only need this if theres no stencil buffer
    //if (dot(baseNormal, baseNormal) > 0.0) {
        vec3 avgNormal = getAverageNormalAndColor(fUV, unScreenResolution, unDirection, baseNormal, baseColor, avgColor);
        if (unShowVariance == 1) {
            oColor = vec4(getNormalVariance(baseNormal, avgNormal) * 20.0, 0.0, 0.0, 1.0);
            oNormal = avgNormal;
        } else if (getNormalVariance(avgNormal, baseNormal) > unNormalThreshold) {
            if (unShowEdges == 1) {
                oColor = vec4(1.0, 0.0, 0.0, 1.0);
            } else {
                oColor = avgColor;
            }
            oNormal = avgNormal;
        } else {
            oColor = baseColor;
            oNormal = baseNormal;
        }
    // Only need this if theres no stencil buffer
    //} else {
    //    discard;
    //}
}