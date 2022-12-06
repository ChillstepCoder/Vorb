uniform sampler2DArray Atlas;
uniform float PlayerZ;

in vec2 fUV;
flat in float fAtlasPage;
in vec3 fPosition;

layout (location = 0) out float fCutout;

const float Z_CUTOFF = 2.0;

void main() {
    float pixelAlpha = texture(Atlas, vec3(fUV, fAtlasPage)).a;
    float a = pixelAlpha * (1.0 - step(Z_CUTOFF, fPosition.z - PlayerZ));
	if (a < 0.9) {
		discard;
	}
	fCutout = a;
}