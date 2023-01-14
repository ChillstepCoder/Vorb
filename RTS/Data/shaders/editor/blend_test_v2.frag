uniform sampler2D unAlbedoFbo;
uniform sampler2D unNormalFbo;
uniform sampler2D unEdgeFbo;
uniform vec2 unScreenResolution;
uniform vec2 unDirection;
uniform int unShowEdges;
//uniform vec2 unCameraZRange;

vec3 sampleNormalAndIncrementTotalWeight(sampler2D image, vec2 uv, float weight, inout float weightTotal, inout vec3 avgColor) {
    vec3 color = texture2D(unAlbedoFbo, uv).rgb;
    vec3 normal = texture2D(image, uv).rgb;
    float isValid = length(normal);
    weightTotal += weight * isValid;
    avgColor += color.rgb * weight * isValid;
    return normal.rgb * weight * isValid;
}

vec3 getAverageNormalAndColor(sampler2D image, vec2 uv, vec2 resolution, vec2 direction, inout vec3 avgColor) {
  vec3 nAvg = vec3(0.0);
  avgColor = vec3(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  
  float weightTotal = 0.0;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv, 0.1964825501511404, weightTotal, avgColor);
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv + (off1 / resolution), 0.2969069646728344, weightTotal, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv - (off1 / resolution), 0.2969069646728344, weightTotal, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv + (off2 / resolution), 0.09447039785044732, weightTotal, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv - (off2 / resolution), 0.09447039785044732, weightTotal, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv + (off3 / resolution), 0.010381362401148057, weightTotal, avgColor).rgb;
  nAvg += sampleNormalAndIncrementTotalWeight(image, uv - (off3 / resolution), 0.010381362401148057, weightTotal, avgColor).rgb;
  
  avgColor /= weightTotal;
  nAvg /= weightTotal;
  return nAvg;
}

in vec2 fUV;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;

void main() {
    // TODO: STENCIL TEST FOR BETTER PERFORMANCE
    vec3 baseColor = texture(unAlbedoFbo, fUV).rgb;
    vec3 baseNormal = texture(unNormalFbo, fUV).rgb;
    float blurIntensity = texture(unEdgeFbo, fUV).r;
    if (blurIntensity > 0.0) {
        vec3 avgColor;
        vec3 avgNormal = getAverageNormalAndColor(unNormalFbo, fUV, unScreenResolution, unDirection, avgColor);
        // Renormalize and scale back to 0-1
        avgNormal = (normalize((avgNormal - 0.5) * 2.0) + 1.0) * 0.5;
        oColor.rgb = mix(baseColor, avgColor, blurIntensity);
        oNormal = mix(baseNormal, avgNormal, blurIntensity);
        if (unShowEdges == 1) {
            oColor.rgb = mix(oColor.rgb, vec3(1.0, 0.0, 0.0), blurIntensity);
        }
    } else {
        oColor.rgb = texture(unAlbedoFbo, fUV).rgb;
        oNormal.rgb = texture(unNormalFbo, fUV).rgb;
    }
    oColor.a = 1.0;
    

}