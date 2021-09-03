// TODO: Indoor mask
uniform sampler2DArray Atlas;
uniform vec3 SunColor;
uniform float SunHeight;
uniform vec4 GradientRect;
uniform float GradientAtlasPage;

in vec2 fUV;

out vec4 fColor;

void main() {
	float sunIntensity = max(SunHeight, 0.0);
	vec2 adjustedUV = fUV;
	adjustedUV.y = min(sunIntensity * adjustedUV.y, 1.0);
	vec3 sunTextureColor = texture(Atlas, vec3(GradientRect.xy + adjustedUV * GradientRect.zw, GradientAtlasPage)).rgb;
    fColor.rgb = sunTextureColor;
	fColor.a = 1.0;
}