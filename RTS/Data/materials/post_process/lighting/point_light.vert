// Input
in vec2 vPosition; // Position in screen space
out vec2 fLocalPosition;
uniform mat4 VP;
uniform vec2 Position;
uniform float Scale;

void main() {
  fLocalPosition = vPosition;
  gl_Position = VP * vec4(vPosition * Scale + Position, 0, 1);
}