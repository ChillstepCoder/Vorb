uniform sampler2D ShadowAlphaAndHeightFbo;
uniform sampler2D ShadowRootScreenYFbo;

in vec2 fUV;

out vec4 fColor;

void main() {
    fColor.rg = texture(ShadowAlphaAndHeightFbo, fUV).rg;
	fColor.b = texture(ShadowRootScreenYFbo, fUV).r;
	fColor.a = 1.0;
}