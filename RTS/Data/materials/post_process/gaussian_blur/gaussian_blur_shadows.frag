uniform sampler2D unInputFbo;
uniform vec2 ScreenResolution;
uniform vec2 unDirection;

in vec2 fUV;

layout (location = 0) out vec4 fColor;

float blur9s(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
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

void main() {
	fColor.r = blur9s(unInputFbo, fUV, ScreenResolution, unDirection).r;
	fColor.a = 1.0;
	//fColor.rgba = blur5(unInputFbo, fUV, ScreenResolution, unDirection).rgba;
}