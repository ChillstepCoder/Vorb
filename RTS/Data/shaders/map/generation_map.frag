
in vec2 fUV;

out vec4 fColor;

void main() {
    fColor = vec4(fUV.x, fUV.y, 0, 1.0);
}