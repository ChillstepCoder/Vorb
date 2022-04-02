uniform sampler2D Texture;

in vec4 fTint;

out vec4 fColor;

void main() {
    fColor = fTint;
}