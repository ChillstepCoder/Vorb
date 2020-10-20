uniform sampler2D Fbo0;
uniform vec3 SunColor;
uniform float SunHeight;

in vec2 fUV;

out vec4 fColor;

float AMBIENT = 0.1;

void main() {
	vec4 pixelColor = texture(Fbo0, fUV);
	float sunIntensity = max(SunHeight, 0.0) * (1.0 - AMBIENT);
    fColor.rgb = pixelColor.rgb * sunIntensity * SunColor + pixelColor.rgb * AMBIENT;
	fColor.a = pixelColor.a;
}