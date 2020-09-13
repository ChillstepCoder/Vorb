uniform sampler2DArray tex;

in vec2 fUV;
flat in float fAtlasPage;
in vec4 fTint;

out vec4 fColor;

void main() {
    fColor = texture(tex, vec3(fUV, fAtlasPage)) * fTint;
    // Don't write 0 alpha (TMP)
    if (fColor.a <= 0.01) {
        discard;
    }
}