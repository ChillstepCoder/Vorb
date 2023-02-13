
uniform sampler2D unFontTexture;

in vec2 fUV;
in vec4 fTint;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
//layout (location = 2) out float oRoughness;

void main() {
    // We are necoding two textures into a sincle uvec4 for efficiency
    oColor = texture(unFontTexture, fUV) * fTint;
    // Don't write 0 alpha (TMP?)
    if (oColor.a < 0.40) {
        discard;
    }
	oColor.a = 0.0;
	oNormal = vec4(0.0, 0.0, 1.0, 1.0);
	
}