uniform sampler2DArray Atlas;
uniform float Time;

in vec2 fUV;
flat in float fAtlasPage;
in vec4 fTint;

layout (location = 0) out vec4 fColor;

void main() {
    fColor = texture(Atlas, vec3(fUV, fAtlasPage)) * fTint;
	fColor.rgb = fColor.rgb * 0.0001 + fTint.rgb;
	fColor.b = (cos(Time) + 1.0) / 2.0;
	fColor.a = 1.0;
	
}