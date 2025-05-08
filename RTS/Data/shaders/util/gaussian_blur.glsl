// https://github.com/Jam3/glsl-fast-gaussian-blur

vec4 blur5(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
  vec4 color = vec4(0.0);
  vec2 off1 = vec2(1.3333333333333333) * direction;
  color += texture2D(image, uv) * 0.29411764705882354;
  color += texture2D(image, uv + (off1 / resolution)) * 0.35294117647058826;
  color += texture2D(image, uv - (off1 / resolution)) * 0.35294117647058826;
  return color; 
}

float blur9r(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
  float color = 0.0;
  vec2 off1 = vec2(1.3846153846) * direction;
  vec2 off2 = vec2(3.2307692308) * direction;
  color += texture2D(image, uv).r * 0.2270270270;
  color += texture2D(image, uv + (off1 / resolution)).r * 0.3162162162;
  color += texture2D(image, uv - (off1 / resolution)).r * 0.3162162162;
  color += texture2D(image, uv + (off2 / resolution)).r * 0.0702702703;
  color += texture2D(image, uv - (off2 / resolution)).r * 0.0702702703;
  return color;
}

vec4 blur9(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
  vec4 color = vec4(0.0);
  vec2 off1 = vec2(1.3846153846) * direction;
  vec2 off2 = vec2(3.2307692308) * direction;
  color += texture2D(image, uv) * 0.2270270270;
  color += texture2D(image, uv + (off1 / resolution)) * 0.3162162162;
  color += texture2D(image, uv - (off1 / resolution)) * 0.3162162162;
  color += texture2D(image, uv + (off2 / resolution)) * 0.0702702703;
  color += texture2D(image, uv - (off2 / resolution)) * 0.0702702703;
  return color;
}

vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
  vec4 color = vec4(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  color += texture2D(image, uv) * 0.1964825501511404;
  color += texture2D(image, uv + (off1 / resolution)) * 0.2969069646728344;
  color += texture2D(image, uv - (off1 / resolution)) * 0.2969069646728344;
  color += texture2D(image, uv + (off2 / resolution)) * 0.09447039785044732;
  color += texture2D(image, uv - (off2 / resolution)) * 0.09447039785044732;
  color += texture2D(image, uv + (off3 / resolution)) * 0.010381362401148057;
  color += texture2D(image, uv - (off3 / resolution)) * 0.010381362401148057;
  return color;
}

vec4 blur13array(sampler2DArray image, vec3 uv, vec2 resolution, vec2 direction) {
  vec4 color = vec4(0.0);
  vec3 off1 = vec3(vec2(1.411764705882353) * direction, 0.0);
  vec3 off2 = vec3(vec2(3.2941176470588234) * direction, 0.0);
  vec3 off3 = vec3(vec2(5.176470588235294) * direction, 0.0);
  vec3 resolution3d = vec3(resolution, 1.0);
  color += texture(image, uv) * 0.1964825501511404;
  color += texture(image, uv + (off1 / resolution3d)) * 0.2969069646728344;
  color += texture(image, uv - (off1 / resolution3d)) * 0.2969069646728344;
  color += texture(image, uv + (off2 / resolution3d)) * 0.09447039785044732;
  color += texture(image, uv - (off2 / resolution3d)) * 0.09447039785044732;
  color += texture(image, uv + (off3 / resolution3d)) * 0.010381362401148057;
  color += texture(image, uv - (off3 / resolution3d)) * 0.010381362401148057;
  return color;
}

vec3 blur13noalpha(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
  vec3 color = vec3(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;
  color += texture2D(image, uv).rgb * 0.1964825501511404;
  color += texture2D(image, uv + (off1 / resolution)).rgb * 0.2969069646728344;
  color += texture2D(image, uv - (off1 / resolution)).rgb * 0.2969069646728344;
  color += texture2D(image, uv + (off2 / resolution)).rgb * 0.09447039785044732;
  color += texture2D(image, uv - (off2 / resolution)).rgb * 0.09447039785044732;
  color += texture2D(image, uv + (off3 / resolution)).rgb * 0.010381362401148057;
  color += texture2D(image, uv - (off3 / resolution)).rgb * 0.010381362401148057;
  return color;
}
