uniform sampler2DArray tex;
uniform float time;

in vec2 fUV;
flat in float fAtlasPage;
in vec4 fTint;
in float depth;

out vec4 fColor;

void main() {
    fColor = texture(tex, vec3(fUV, fAtlasPage)) * fTint;
	fColor.rgb = fColor.rgb * 0.0000001 + vec3(depth, depth, depth);
    // Don't write 0 alpha (TMP)
    if (fColor.a <= 0.01) {
        discard;
    }
}