uniform sampler2D Fbo0;

in vec2 fUV;

out vec4 fColor;

void main() {
    fColor = texture(Fbo0, fUV);
}