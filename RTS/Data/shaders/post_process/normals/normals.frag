uniform sampler2D FboNormals;

in vec2 fUV;

out vec4 fColor;

void main() {
	// Sample neighbors
	fColor = texture(FboNormals, fUV);
	fColor.a = 1.0;
}