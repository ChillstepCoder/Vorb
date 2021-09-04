// Input
in vec4 vPosition; // Position in screen space
in vec4 vTint;

uniform mat4 VP;

out vec4 fTint;

void main() {
  fTint = vTint;
  gl_Position = VP * vPosition;
}