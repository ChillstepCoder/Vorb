// Input
in vec2 vPosition; // Position in screen space
out vec2 fLocalPosition;

void main() {
  fLocalPosition = vPosition;
  gl_Position =  vec4(vPosition, 0, 1);
}