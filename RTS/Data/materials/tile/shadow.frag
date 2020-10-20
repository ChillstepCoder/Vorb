uniform sampler2DArray Atlas;

in vec2 fUV;
flat in float fAtlasPage;
in float fShadowHeight;

layout (location = 0) out float fOutputHeight;

void main() {
    float alpha = texture(Atlas, vec3(fUV, fAtlasPage)).a;
	if (alpha <= 0.0) {
	    discard;
	}
	fOutputHeight = fShadowHeight * alpha;
}