uniform sampler2D Fbo0;
uniform sampler2D FboLight;

in vec2 fUV;

out vec4 fColor;

void main() {
    fColor.rgb = texture(FboLight, fUV).rgb * texture(Fbo0, fUV).rgb;
	fColor.a = 1.0;
}