
uniform sampler2D unFontTexture;

in vec2 fUV;
uniform vec4 unColor = vec4(1.0,1.0,1.0,1.0);

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
//layout (location = 2) out float oRoughness;

void main() {
    oColor = texture(unFontTexture, fUV) * unColor;
    // Don't write 0 alpha (TMP?)
    if (oColor.a < 0.40) {
        discard;
    }
	oColor.a = 0.0;
	oNormal = vec4(0.0, 0.0, 1.0, 1.0);
	
}