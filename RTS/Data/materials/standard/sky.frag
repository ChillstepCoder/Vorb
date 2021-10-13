// TODO: Indoor mask
uniform float SunHeight;
uniform sampler2D StarfieldTexture;
uniform float Time;
//uniform vec4 GradientRect;
//uniform float GradientAtlasPage;

in vec2 fUV;
in vec3 fPosition;

out vec4 fColor;

void main() {
	// Add UV based on rotation so the sky rotates (tiling)
	// Zangle is between 0 and 2PI
	float SunIntensity = max(SunHeight, 0.0);
	vec4 starsColor = texture(StarfieldTexture, fUV).rgba;
	float positionInput = (fPosition.z - fPosition.x - fPosition.y) * 700.0;
	float sparkle = (sin(Time * 1.25 + positionInput) + 1.3) * 0.434782;
	float starIntensity = pow(1.0 - SunIntensity, 20.0);
	fColor.rgb += starsColor.rgb * starsColor.a * sparkle * starIntensity;
	fColor.a = 1.0;
}