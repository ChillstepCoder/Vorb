uniform sampler2DArray Atlas;

in vec2 fUV;
flat in float fAtlasPage;
in vec4 fTint;

layout (location = 0) out vec4 fColor;
layout (location = 1) out vec4 fNormal;

void main() {
    fColor = texture(Atlas, vec3(fUV, fAtlasPage)) * fTint;
    // Don't write 0 alpha (TMP?)
	// TODO: Noise on this edge so that its fuzzy average
	
    if (fColor.a < 0.85) {
        discard;
    }
	fColor.a = 1.0;
	
	
	// Normal is always the next page
	fNormal = texture(Atlas, vec3(fUV, fAtlasPage + 1.0));
	fNormal.a = fColor.a;
}