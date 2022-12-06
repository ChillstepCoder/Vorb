
const float ALPHA_THRESHOLD = 0.01;
const float SAMPLE_COUNT = 7.0;

// Only increment count when there is alpha
vec3 sampleAndIncrementTotalAlpha(sampler2D image, vec2 uv, float weight, inout float weightTotal) {
    vec4 color = texture2D(image, uv);
    float alphaMult = step(ALPHA_THRESHOLD, color.a);
    weightTotal += weight * alphaMult;
    return color.rgb * weight * alphaMult;
}

vec3 blur13rgbAlphaMask(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
  vec3 color = vec3(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  
  float weightTotal = 0.0;
  color += sampleAndIncrementTotalAlpha(image, uv, 0.1964825501511404, weightTotal);
  color += sampleAndIncrementTotalAlpha(image, uv + (off1 / resolution), 0.2969069646728344, weightTotal).rgb;
  color += sampleAndIncrementTotalAlpha(image, uv - (off1 / resolution), 0.2969069646728344, weightTotal).rgb;
  color += sampleAndIncrementTotalAlpha(image, uv + (off2 / resolution), 0.09447039785044732, weightTotal).rgb;
  color += sampleAndIncrementTotalAlpha(image, uv - (off2 / resolution), 0.09447039785044732, weightTotal).rgb;
  color += sampleAndIncrementTotalAlpha(image, uv + (off3 / resolution), 0.010381362401148057, weightTotal).rgb;
  color += sampleAndIncrementTotalAlpha(image, uv - (off3 / resolution), 0.010381362401148057, weightTotal).rgb;
  
  color /= weightTotal;
  return color;
}

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * CameraZRange.x * CameraZRange.y / (CameraZRange.y + CameraZRange.x - zn * (CameraZRange.y - CameraZRange.x));
}

const float DEPTH_BLEND_START = 50.0;
const float DEPTH_BLEND_END = 300.0;

// Only increment count when there is alpha
vec3 sampleAndIncrementTotalDepth(sampler2D image, sampler2D depthTex, float baseDepth, vec2 uv, float weight, inout float weightTotal) {
    float depth = linearizeDepth(texture2D(depthTex, uv).r);
	float depthMult = 1.0 - smoothstep(DEPTH_BLEND_START, DEPTH_BLEND_END, depth - baseDepth);
    vec3 color = texture2D(image, uv).rgb;
    weightTotal += weight * depthMult;
    return color.rgb * weight * depthMult;
}


vec3 blur13rgbDepthMask(sampler2D image, sampler2D depthTex, float baseDepth, vec2 uv, vec2 resolution, vec2 direction) {
  vec3 color = vec3(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  
  float linDepth = linearizeDepth(baseDepth);
  
  float weightTotal = 0.0;
  color += sampleAndIncrementTotalDepth(image, depthTex, linDepth, uv, 0.1964825501511404, weightTotal);
  color += sampleAndIncrementTotalDepth(image, depthTex, linDepth, uv + (off1 / resolution), 0.2969069646728344, weightTotal).rgb;
  color += sampleAndIncrementTotalDepth(image, depthTex, linDepth, uv - (off1 / resolution), 0.2969069646728344, weightTotal).rgb;
  color += sampleAndIncrementTotalDepth(image, depthTex, linDepth, uv + (off2 / resolution), 0.09447039785044732, weightTotal).rgb;
  color += sampleAndIncrementTotalDepth(image, depthTex, linDepth, uv - (off2 / resolution), 0.09447039785044732, weightTotal).rgb;
  color += sampleAndIncrementTotalDepth(image, depthTex, linDepth, uv + (off3 / resolution), 0.010381362401148057, weightTotal).rgb;
  color += sampleAndIncrementTotalDepth(image, depthTex, linDepth, uv - (off3 / resolution), 0.010381362401148057, weightTotal).rgb;
  
  color /= weightTotal;
  return color;
}