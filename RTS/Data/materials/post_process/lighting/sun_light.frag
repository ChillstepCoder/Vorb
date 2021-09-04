// TODO: Indoor mask
uniform vec3 SunColor;
uniform float SunHeight;
uniform sampler2D FboDepth;

in vec2 fUV;

out vec4 fColor;

float AMBIENT = 0.2;

void main() {
	float depth = texture(FboDepth, fUV).r;
	float isSky = step(0.999999999, depth);
	float isGround = 1.0 - isSky;
	
	float sunIntensity = max(SunHeight, 0.0) * (1.0 - AMBIENT);
	float lightTotal = sunIntensity + AMBIENT;
    fColor.rgb = isGround * lightTotal * SunColor + isSky;
	fColor.a = 1.0;
}