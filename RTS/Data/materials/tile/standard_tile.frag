uniform sampler2DArray Atlas;

in vec2 fUV;
flat in float fAtlasPage;
in vec4 fTint;

layout (location = 0) out vec4 fColor;
layout (location = 1) out vec4 fNormal;

void main() {
    fColor = texture(Atlas, vec3(fUV, fAtlasPage)) * fTint;
    // Don't write 0 alpha (TMP)
    if (fColor.a <= 0.99) {
        discard;
    }
	// Normal is always the next page
	fNormal= texture(Atlas, vec3(fUV, fAtlasPage + 1.0));
}