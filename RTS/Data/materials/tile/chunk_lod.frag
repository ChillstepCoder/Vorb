uniform sampler2D Texture;

in vec2 fUV;

out vec4 fColor;

void main() {
    fColor = texture(Texture, fUV);
	fColor.a = 1.0;
}