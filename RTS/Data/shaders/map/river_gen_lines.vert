
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec4 vColor;

// Output
out vec4 fColor;

uniform mat4 unVP;
uniform vec2 unCameraPos;

void main() {
    fColor = vColor;
    gl_Position = unVP * vec4(vPosition.xy - unCameraPos * 2.0, 0,1);
}