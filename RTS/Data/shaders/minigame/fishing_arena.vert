uniform mat4 unVP;
uniform float unRadius;

in vec2 vPosition;


out vec2 fUV;

void main() {
    fUV = (vPosition + 1.0) * 0.5;
    // Offset to center
    gl_Position = unVP * vec4(vPosition * unRadius, 0.0, 1.0) + vec4(1.0, -1.0, 0.0, 0.0);
}
