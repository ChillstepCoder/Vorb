// Input
in vec2 vPosition; // Position in screen space

// Output
out vec2 fLocalPosition;
out vec2 fUvNormals;

// Uniforms
uniform mat4 VP;
uniform vec2 Position;
uniform float Scale;

void main() {
  fLocalPosition = vPosition;
  gl_Position = VP * vec4(vPosition * Scale + Position, 0, 1);
  fUvNormals = (gl_Position.xy + 1.0) / 2.0;
}