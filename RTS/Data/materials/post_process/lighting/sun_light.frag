// TODO: Indoor mask
uniform vec3 SunColor;
uniform float SunHeight;

out vec4 fColor;

float AMBIENT = 0.2;

void main() {
	float sunIntensity = max(SunHeight, 0.0) * (1.0 - AMBIENT);
	float lightTotal = sunIntensity + AMBIENT;
    fColor.rgb = lightTotal * SunColor;
	fColor.a = 1.0;
}